// altimeter.cxx - an altimeter tied to the static port.
// Written by David Megginson, started 2002.
// Modified by John Denker in 2007 to use a two layer atmosphere
// model in src/Environment/atmosphere.?xx
// Last modified by Eric van den Berg, 25 Nov 2012
//
// This file is in the Public Domain and comes with no warranty.

// Example invocation, in the instrumentation.xml file:
//      <altimeter>
//        <name>encoder</name>
//        <number>0</number>
//        <static-pressure>/systems/static/pressure-inhg</static-pressure>
//        <quantum>10</quantum>
//        <tau>0</tau>
//      </altimeter>
// Note non-default name, quantum, and tau values.

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/constants.h>
#include <simgear/math/interpolater.hxx>
#include <simgear/math/SGMath.hxx>

#include <Main/fg_props.hxx>
#include <Main/util.hxx>
#include <Environment/atmosphere.hxx>

#include "altimeter.hxx"

Altimeter::Altimeter ( SGPropertyNode *node, const std::string& aDefaultName, double quantum ) :
      _name(node->getStringValue("name", aDefaultName.c_str())),
      _num(node->getIntValue("number", 0)),
      _static_pressure(node->getStringValue("static-pressure", "/systems/static/pressure-inhg")),
      _tau(node->getDoubleValue("tau", 0.1)),
      _quantum(node->getDoubleValue("quantum", quantum)),
      _settingInHg(29.921260)
{
    // FIXME: change default to false once all aircraft which use
    // altimiter as an encoder are converted to request this explicitly
    _encodeModeC = node->getBoolValue("encode-mode-c", true);
    _encodeModeS = node->getBoolValue("encode-mode-s", false);
    
    _tiedProperties.setRoot( _rootNode );
}

Altimeter::~Altimeter ()
{}

double
Altimeter::getSettingInHg() const
{
    return _settingInHg;
}

void
Altimeter::setSettingInHg( double value )
{
    _settingInHg = value;
}

double
Altimeter::getSettingHPa() const
{
    return _settingInHg * SG_INHG_TO_PA / 100;
}

void
Altimeter::setSettingHPa( double value )
{
    _settingInHg = value * SG_PA_TO_INHG * 100;
}


void
Altimeter::init ()
{
    _pressure_node     = fgGetNode(_static_pressure, true);
    _serviceable_node  = _rootNode->getChild("serviceable", 0, true);
    _press_alt_node    = _rootNode->getChild("pressure-alt-ft", 0, true);
    if (_encodeModeC) {
        _mode_c_node = _rootNode->getChild("mode-c-alt-ft", 0, true);
    }
    
    if (_encodeModeS) {
        _mode_s_node = _rootNode->getChild("mode-s-alt-ft", 0, true);
    }
    
    _altitude_node     = _rootNode->getChild("indicated-altitude-ft", 0, true);

    reinit();
}

void
Altimeter::reinit ()
{
    _raw_PA = 0.0;
    _kollsman = 0.0;
}

void
Altimeter::bind()
{
    _rootNode = fgGetNode("/instrumentation/" + _name, _num, true );
    _tiedProperties.setRoot(_rootNode);
    
    _tiedProperties.Tie("setting-inhg", this, &Altimeter::getSettingInHg, &Altimeter::setSettingInHg );
    _tiedProperties.Tie("setting-hpa", this, &Altimeter::getSettingHPa, &Altimeter::setSettingHPa );
}

void
Altimeter::unbind()
{
    _tiedProperties.Untie();
}

void
Altimeter::update (double dt)
{
    if (_serviceable_node->getBoolValue()) {
        double trat = _tau > 0 ? dt/_tau : 100;
        double pressure = _pressure_node->getDoubleValue();
        double press_alt = _press_alt_node->getDoubleValue();
        // The mechanism settles slowly toward new pressure altitude:
        _raw_PA = fgGetLowPass(_raw_PA, _altimeter.press_alt_ft(pressure), trat);
        
        if (_encodeModeC) {
            _mode_c_node->setDoubleValue(100 * SGMiscd::round(_raw_PA/100));
        }
        
        if (_encodeModeS) {
            _mode_s_node->setDoubleValue(10 * SGMiscd::round(_raw_PA/10));
        }

        _kollsman = fgGetLowPass(_kollsman, _altimeter.kollsman_ft(_settingInHg), trat);
        if (_quantum)
            press_alt = _quantum * SGMiscd::round(_raw_PA/_quantum);
        else
            press_alt = _raw_PA;
        _press_alt_node->setDoubleValue(press_alt);
        _altitude_node->setDoubleValue(press_alt - _kollsman);
    }
}


// Register the subsystem.
#if 0
SGSubsystemMgr::InstancedRegistrant<Altimeter> registrantAltimeter(
    SGSubsystemMgr::FDM,
    {{"instrumentation", SGSubsystemMgr::Dependency::HARD}});
#endif

// end of altimeter.cxx
