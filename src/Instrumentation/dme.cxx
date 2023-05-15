// dme.cxx - distance-measuring equipment.
// Written by David Megginson, started 2003.
//
// This file is in the Public Domain and comes with no warranty.

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/compiler.h>
#include <simgear/sg_inlines.h>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/math/sg_random.hxx>
#include <simgear/sound/sample_group.hxx>

#include <Main/fg_props.hxx>
#include <Navaids/navlist.hxx>
#include <Sound/audioident.hxx>

#include "dme.hxx"

#include <cstdio>


/**
 * Adjust the range.
 *
 * Start by calculating the radar horizon based on the elevation
 * difference, then clamp to the maximum, then add a fudge for
 * borderline reception.
 */
static double
adjust_range (double transmitter_elevation_ft, double aircraft_altitude_ft,
              double max_range_nm)
{
    double delta_elevation_ft =
        fabs(aircraft_altitude_ft - transmitter_elevation_ft);
    double range_nm = 1.23 * sqrt(delta_elevation_ft);
    if (range_nm > max_range_nm)
        range_nm = max_range_nm;
    else if (range_nm < 20.0)
        range_nm = 20.0;
    double rand = sg_random();
    return range_nm + (range_nm * rand * rand);
}

namespace {
  
  class DMEFilter : public FGNavList::TypeFilter
  {
  public:
    DMEFilter() :
      TypeFilter(FGPositioned::DME),
      _locEnabled(fgGetBool("/sim/realism/dme-fallback-to-loc", true))
    {
      if (_locEnabled) {
        _mintype = FGPositioned::ILS;
      }
    }
    
    virtual bool pass(FGPositioned* pos) const
    {
      switch (pos->type()) {
      case FGPositioned::DME: return true;
      case FGPositioned::ILS:
      case FGPositioned::LOC: return _locEnabled;
      default: return false;
      }
    }
    
  private:
    const bool _locEnabled;
  };
  
} // of anonymous namespace


DME::DME ( SGPropertyNode *node )
    : _last_distance_nm(0),
      _last_frequency_mhz(-1),
      _time_before_search_sec(0),
      _navrecord(NULL),
      _audioIdent(NULL)
{
    readConfig(node, "dme");
}

DME::~DME ()
{
    delete _audioIdent;
}

void
DME::init ()
{
    std::string branch = nodePath();
    SGPropertyNode *node = fgGetNode(branch, true );
    initServicePowerProperties(node);
    
    SGPropertyNode *fnode = node->getChild("frequencies", 0, true);
    _source_node = fnode->getChild("source", 0, true);
    _frequency_node = fnode->getChild("selected-mhz", 0, true);
    _in_range_node = node->getChild("in-range", 0, true);
    _distance_node = node->getChild("indicated-distance-nm", 0, true);
    _speed_node = node->getChild("indicated-ground-speed-kt", 0, true);
    _time_node = node->getChild("indicated-time-min", 0, true);

    double d = node->getDoubleValue( "volume", 1.0 );
    _volume_node = node->getChild("volume", 0, true);
    _volume_node->setDoubleValue( d );

    bool b = node->getBoolValue( "ident", false );
    _ident_btn_node = node->getChild("ident", 0, true);
    _ident_btn_node->setBoolValue( b );

    SGPropertyNode *subnode = node->getChild("KDI572-574", 0, true);

    _distance_string = subnode->getChild("nm",0, true);
    _distance_string->setStringValue("---");
    _speed_string = subnode->getChild("kt", 0, true);
    _speed_string->setStringValue("---");
    _time_string = subnode->getChild("min",0, true);
    _time_string->setStringValue("--");

    std::ostringstream temp;
    temp << name() << "-ident-" << number();
    if( NULL == _audioIdent ) 
        _audioIdent = new DMEAudioIdent(temp.str());
    _audioIdent->init();

    reinit();
}

void
DME::reinit ()
{
    _time_before_search_sec = 0;
	clear();
}

void
DME::update (double delta_time_sec)
{
    if( delta_time_sec < SGLimitsd::min() )
        return;  //paused
    char tmp[16];
    
    // Figure out the source
    string source = _source_node->getStringValue();
    if (source.empty()) {
        std::string branch;
        branch = "/instrumentation/" + name() + "/frequencies/selected-mhz";
        _source_node->setStringValue(branch);
        source = _source_node->getStringValue();
    }
                                // Get the frequency

    double frequency_mhz = fgGetDouble(source, 108.0);
    if (frequency_mhz != _last_frequency_mhz) {
        _time_before_search_sec = 0;
        _last_frequency_mhz = frequency_mhz;
    }
    _frequency_node->setDoubleValue(frequency_mhz);

                                // Get the aircraft position
    // On timeout, scan again
    _time_before_search_sec -= delta_time_sec;
    if (_time_before_search_sec < 0) {
        _time_before_search_sec = 1.0;

      SGGeod pos(globals->get_aircraft_position());
      DMEFilter filter;
      _navrecord = FGNavList::findByFreq(frequency_mhz, pos, &filter);
    }

    // If it's off, don't bother.
    if (!isServiceableAndPowered()) {
        clear();
        return;
    }
    
    // If it's on, but invalid source,don't bother.
	if (nullptr == _navrecord) {
		clear();
        return;
    }

    // Calculate the distance to the transmitter
    double distance_nm = dist(_navrecord->cart(),
                  globals->get_aircraft_position_cart()) * SG_METER_TO_NM;

    double range_nm = adjust_range(_navrecord->get_elev_ft(),
                                   globals->get_aircraft_position().getElevationFt(),
                                   _navrecord->get_range());

    if (distance_nm <= range_nm) {
        double volume = _volume_node->getDoubleValue();
        if( !_ident_btn_node->getBoolValue() )
            volume = 0.0;

        _audioIdent->setIdent(_navrecord->ident(), volume );

        double speed_kt = (fabs(distance_nm - _last_distance_nm) *
                           ((1 / delta_time_sec) * 3600.0));
        _last_distance_nm = distance_nm;

        _in_range_node->setBoolValue(true);
        double tmp_dist = distance_nm - _navrecord->get_multiuse();
        if ( tmp_dist < 0.0 ) {
            tmp_dist = 0.0;
        }
        _distance_node->setDoubleValue( tmp_dist );
        if ( tmp_dist >389 ) tmp_dist = 389;
        if ( tmp_dist >= 100.0) {
            snprintf ( tmp,16,"%3.0f",tmp_dist);
        } else {
            snprintf ( tmp,16,"%2.1f",tmp_dist);
        }
        _distance_string->setStringValue(tmp);

        _speed_node->setDoubleValue(speed_kt);
        double spd = speed_kt;
        if(spd>999) spd=999;
        snprintf ( tmp,16,"%3.0f",spd);
        _speed_string->setStringValue(tmp);


        if (SGLimitsd::min() < fabs(speed_kt)){
            double tm = distance_nm/speed_kt*60.0;
            _time_node->setDoubleValue(tm);
            if (tm >99) tm= 99;
            snprintf ( tmp,16,"%2.0f",tm);
            _time_string->setStringValue(tmp);
        }
        
    } else {
		clear();
    }

    _audioIdent->update( delta_time_sec );
}

void DME::clear()
{
	_last_distance_nm = 0;
	_in_range_node->setBoolValue(false);
	_distance_node->setDoubleValue(0);
	_distance_string->setStringValue("---");
	_speed_node->setDoubleValue(0);
	_speed_string->setStringValue("---");
	_time_node->setDoubleValue(0);
	_time_string->setStringValue("--");
	_audioIdent->setIdent("", 0.0);
}

// Register the subsystem.
#if 0
SGSubsystemMgr::InstancedRegistrant<DME> registrantDME(
    SGSubsystemMgr::FDM,
    {{"instrumentation", SGSubsystemMgr::Dependency::HARD}},
    1.0);
#endif

// end of dme.cxx
