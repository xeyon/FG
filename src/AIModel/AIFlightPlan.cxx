// // FGAIFlightPlan - class for loading and storing  AI flight plans
// Written by David Culp, started May 2004
// - davidculp2@comcast.net
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <iterator>
#include <algorithm>

#include <simgear/constants.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Main/fg_init.hxx>
#include <Airports/airport.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/runways.hxx>
#include <Airports/groundnetwork.hxx>

#include <Environment/environment_mgr.hxx>
#include <Environment/environment.hxx>

#include <Traffic/Schedule.hxx>

#include "AIFlightPlan.hxx"
#include "AIAircraft.hxx"

using std::string;

FGAIWaypoint::FGAIWaypoint() {
  speed       = 0;
  crossat     = 0;
  finished    = 0;
  gear_down   = 0;
  flaps       = 0;
  on_ground   = 0;
  routeIndex  = 0;
  time_sec    = 0;
  trackLength = 0;
}

bool FGAIWaypoint::contains(const string& target) {
    size_t found = name.find(target);
    if (found == string::npos)
        return false;
    else
        return true;
}

double FGAIWaypoint::getLatitude()
{
  return pos.getLatitudeDeg();
}

double FGAIWaypoint::getLongitude()
{
  return pos.getLongitudeDeg();
}

double FGAIWaypoint::getAltitude()
{
  return pos.getElevationFt();
}

void FGAIWaypoint::setLatitude(double lat)
{
  pos.setLatitudeDeg(lat);
}

void FGAIWaypoint::setLongitude(double lon)
{
  pos.setLongitudeDeg(lon);
}

void FGAIWaypoint::setAltitude(double alt)
{
  pos.setElevationFt(alt);
}

FGAIFlightPlan::FGAIFlightPlan() :
    sid(NULL),
    repeat(false),
    distance_to_go(0),
    lead_distance_ft(0),
    leadInAngle(0),
    start_time(0),
    arrivalTime(0),
    leg(0),
    lastNodeVisited(0),
    isValid(true)
{
    wpt_iterator    = waypoints.begin();
}

FGAIFlightPlan::FGAIFlightPlan(const string& filename) :
    sid(NULL),
    repeat(false),
    distance_to_go(0),
    lead_distance_ft(0),
    leadInAngle(0),
    start_time(0),
    arrivalTime(0),
    leg(10),
    lastNodeVisited(0),
    isValid(parseProperties(filename))
{
}


/**
 * This is a modified version of the constructor,
 * Which not only reads the waypoints from a
 * Flight plan file, but also adds the current
 * Position computed by the traffic manager, as well
 * as setting speeds and altitude computed by the
 * traffic manager.
 */
FGAIFlightPlan::FGAIFlightPlan(FGAIAircraft *ac,
                               const std::string& p,
                               double course,
                               time_t start,
                               time_t remainingTime,
                               FGAirport *dep,
                               FGAirport *arr,
                               bool firstLeg,
                               double radius,
                               double alt,
                               double lat,
                               double lon,
                               double speed,
                               const string& fltType,
                               const string& acType,
                               const string& airline) :
    sid(NULL),
    repeat(false),
    distance_to_go(0),
    lead_distance_ft(0),
    leadInAngle(0),
    start_time(start),
    arrivalTime(0),
    leg(10),
    lastNodeVisited(0),
    isValid(false),
    departure(dep),
    arrival(arr)
{
  if (parseProperties(p)) {
    isValid = true;
  } else {
    createWaypoints(ac, course, start, remainingTime, dep, arr, firstLeg, radius,
                    alt, lat, lon, speed, fltType, acType, airline);
  }
}

FGAIFlightPlan::~FGAIFlightPlan()
{
  deleteWaypoints();
  //delete taxiRoute;
}

void FGAIFlightPlan::createWaypoints(FGAIAircraft *ac,
                                     double course,
                                     time_t start,
                                     time_t remainingTime,
                                     FGAirport *dep,
                                     FGAirport *arr,
                                     bool firstLeg,
                                     double radius,
                                     double alt,
                                     double lat,
                                     double lon,
                                     double speed,
                                     const string& fltType,
                                     const string& acType,
                                     const string& airline)
{
  time_t now = globals->get_time_params()->get_cur_time();
  time_t timeDiff = now-start;
  leg = AILeg::STARTUP_PUSHBACK;

  if ((timeDiff > 60) && (timeDiff < 1500))
    leg = AILeg::TAXI;
  else if ((timeDiff >= 1500) && (timeDiff < 2000))
    leg = AILeg::TAKEOFF;
  else if (timeDiff >= 2000) {
    if (remainingTime > 2000) {
      leg = AILeg::CRUISE;
    } else {
      leg = AILeg::APPROACH;
    }
  }

  SG_LOG(SG_AI, SG_DEBUG, ac->getTrafficRef()->getCallSign() << "|Route from " << dep->getId() << " to " << arr->getId() <<
           ". Set leg to : " << leg << " " << remainingTime);

  wpt_iterator = waypoints.begin();
  bool dist = 0;
  isValid = create(ac, dep, arr, leg, alt, speed, lat, lon,
                   firstLeg, radius, fltType, acType, airline, dist);
  wpt_iterator = waypoints.begin();
}

bool FGAIFlightPlan::parseProperties(const std::string& filename)
{
    SGPath fp = globals->findDataPath("AI/FlightPlans/" + filename);
    if (!fp.exists()) {
        return false;
    }

    return readFlightplan(fp);
}

bool FGAIFlightPlan::readFlightplan(const SGPath& file)
{
    sg_ifstream f(file);
    return readFlightplan(f, file);
}

bool FGAIFlightPlan::readFlightplan(std::istream& stream, const sg_location& loc)
{
    SGPropertyNode root;
    try {
        readProperties(stream, &root);
    } catch (const sg_exception& e) {
        SG_LOG(SG_AI, SG_ALERT, "Error reading AI flight plan: " << loc.asString() << " message:" << e.getFormattedMessage());
        return false;
    }

    SGPropertyNode* node = root.getNode("flightplan");
    if (!node) {
        SG_LOG(SG_AI, SG_ALERT, "Error reading AI flight plan: " << loc.asString() << ": no <flightplan> root element");
        return false;
    }

    for (int i = 0; i < node->nChildren(); i++) {
        FGAIWaypoint* wpt = new FGAIWaypoint;
        SGPropertyNode* wpt_node = node->getChild(i);

        bool gear, flaps;

        // Calculate some default values if they are not set explicitly in the flightplan
        if (wpt_node->getDoubleValue("ktas", 0) < 1.0f) {
            // Not moving so assume shut down.
            wpt->setPowerDownLights();
            flaps = false;
            gear = true;
        } else if (wpt_node->getDoubleValue("alt", 0) > 10000.0f) {
            // Cruise flight;
            wpt->setCruiseLights();
            flaps = false;
            gear = false;
        } else if (wpt_node->getBoolValue("on-ground", false)) {
            // On ground
            wpt->setGroundLights();
            flaps = true;
            gear = true;
        } else if (wpt_node->getDoubleValue("alt", 0) < 3000.0f) {
            // In the air below 3000 ft, so flaps and gear down.
            wpt->setApproachLights();
            flaps = true;
            gear = true;
        } else {
            // In the air 3000-10000 ft
            wpt->setApproachLights();
            flaps = false;
            gear = false;
        }

        wpt->setName(wpt_node->getStringValue("name", "END"));
        wpt->setLatitude(wpt_node->getDoubleValue("lat", 0));
        wpt->setLongitude(wpt_node->getDoubleValue("lon", 0));
        wpt->setAltitude(wpt_node->getDoubleValue("alt", 0));
        wpt->setSpeed(wpt_node->getDoubleValue("ktas", 0));
        wpt->setCrossat(wpt_node->getDoubleValue("crossat", -10000));
        wpt->setGear_down(wpt_node->getBoolValue("gear-down", gear));
        wpt->setFlaps(wpt_node->getBoolValue("flaps-down", flaps) ? 1.0 : 0.0);
        wpt->setSpoilers(wpt_node->getBoolValue("spoilers", false) ? 1.0 : 0.0);
        wpt->setSpeedBrakes(wpt_node->getBoolValue("speedbrakes", false) ? 1.0 : 0.0);
        wpt->setOn_ground(wpt_node->getBoolValue("on-ground", false));
        wpt->setTime_sec(wpt_node->getDoubleValue("time-sec", 0));
        wpt->setTime(wpt_node->getStringValue("time", ""));
        wpt->setFinished((wpt->getName() == "END"));
        pushBackWaypoint(wpt);
    }

    auto lastWp = getLastWaypoint();
    if (!lastWp || lastWp->getName().compare("END") != 0) {
        SG_LOG(SG_AI, SG_ALERT, "FGAIFlightPlan::Flightplan (" + loc.asString() + ") missing END node");
        return false;
    }


    wpt_iterator = waypoints.begin();
    return true;
}

FGAIWaypoint* FGAIFlightPlan::getLastWaypoint()
{
    if (waypoints.empty())
        return nullptr;

    return waypoints.back();
};

FGAIWaypoint* FGAIFlightPlan::getPreviousWaypoint( void ) const
{
    if (empty())
        return nullptr;

    if (wpt_iterator == waypoints.begin()) {
        return nullptr;
    } else {
        wpt_vector_iterator prev = wpt_iterator;
        return *(--prev);
    }
}

FGAIWaypoint* FGAIFlightPlan::getCurrentWaypoint( void ) const
{
  if (wpt_iterator == waypoints.end())
      return nullptr;
  return *wpt_iterator;
}

FGAIWaypoint* FGAIFlightPlan::getNextWaypoint( void ) const
{
    if (wpt_iterator == waypoints.end())
        return nullptr;

    wpt_vector_iterator last = waypoints.end() - 1;
    if (wpt_iterator == last) {
        return nullptr;
    } else {
        return *(wpt_iterator + 1);
    }
}

int FGAIFlightPlan::getNextTurnAngle( void ) const
{
    return nextTurnAngle;
}


void FGAIFlightPlan::IncrementWaypoint(bool eraseWaypoints )
{
    if (empty())
        return;

    if (eraseWaypoints)
    {
        if (wpt_iterator == waypoints.begin())
            ++wpt_iterator;
        else
        if (!waypoints.empty())
        {
            delete *(waypoints.begin());
            waypoints.erase(waypoints.begin());
            wpt_iterator = waypoints.begin();
            ++wpt_iterator;
        }
    }
    else {
      ++wpt_iterator;
    }
    // Calculate the angle of the next turn.
    if (wpt_iterator == waypoints.end())
        return;
    if (wpt_iterator == waypoints.begin())
        return;
    if (wpt_iterator+1 == waypoints.end())
        return;
    if (waypoints.size()<3)
        return;
    FGAIWaypoint* previousWP = *(wpt_iterator -1);
    FGAIWaypoint* currentWP = *(wpt_iterator);
    FGAIWaypoint* nextWP = *(wpt_iterator + 1);
    int currentBearing = this->getBearing(previousWP, currentWP);
    int nextBearing = this->getBearing(currentWP, nextWP);

    nextTurnAngle = SGMiscd::normalizePeriodic(-180, 180, nextBearing - currentBearing);
    if ((previousWP->getSpeed() > 0 && nextWP->getSpeed() < 0) ||
        (previousWP->getSpeed() < 0 && nextWP->getSpeed() > 0)) {
      nextTurnAngle += 180;
      SG_LOG(SG_AI, SG_BULK, "Add 180 to turn angle pushback end");
    }
    SG_LOG(SG_AI, SG_BULK, "Calculated next turn angle " << nextTurnAngle << " " << previousWP->getName() << " " << currentWP->getName() << " Previous Speed " << previousWP->getSpeed() << " Next Speed " << nextWP->getSpeed());
}

void FGAIFlightPlan::DecrementWaypoint()
{
    if (empty())
        return;

    --wpt_iterator;
}

void FGAIFlightPlan::eraseLastWaypoint()
{
    if (empty())
        return;

    delete (waypoints.back());
    waypoints.pop_back();;
    wpt_iterator = waypoints.begin();
    ++wpt_iterator;
}

// gives distance in meters from a position to a waypoint
double FGAIFlightPlan::getDistanceToGo(double lat, double lon, FGAIWaypoint* wp) const{
  return SGGeodesy::distanceM(SGGeod::fromDeg(lon, lat), wp->getPos());
}

/**
  sets distance in feet from a lead point to the current waypoint
  basically a catch radius, that triggers the advancement of WPs
*/
void FGAIFlightPlan::setLeadDistance(double speed,
                                     double bearing,
                                     FGAIWaypoint* current,
                                     FGAIWaypoint* next){
  double turn_radius_m;
  // Handle Ground steering
  // At a turn rate of 30 degrees per second, it takes 12 seconds to do a full 360 degree turn
  // So, to get an estimate of the turn radius, calculate the cicumference of the circle
  // we travel on. Get the turn radius by dividing by PI (*2).
  // FIXME Why when going backwards? No fabs
  if (speed < 0.5) {
    setLeadDistance(0.5);
    return;
  }
  if (speed > 0 && speed < 0.5) {
    setLeadDistance(5 * SG_METER_TO_FEET);
    SG_LOG(SG_AI, SG_BULK, "Setting Leaddistance fixed " << (lead_distance_ft*SG_FEET_TO_METER));
    return;
  }

  double speed_mps = speed * SG_KT_TO_MPS;
  if (speed < 25) {
    turn_radius_m = ((360/30)*fabs(speed_mps)) / (2*M_PI);
  } else {
    turn_radius_m = 0.1911 * speed * speed; // an estimate for 25 degrees bank
  }

  double inbound = bearing;
  double outbound = getBearing(current, next);
  leadInAngle = fabs(inbound - outbound);
  if (leadInAngle > 180.0) leadInAngle = 360.0 - leadInAngle;
  //if (leadInAngle < 30.0) // To prevent lead_dist from getting so small it is skipped
  //  leadInAngle = 30.0;

  //lead_distance_ft = turn_radius * sin(leadInAngle * SG_DEGREES_TO_RADIANS);

  if ((int)leadInAngle==0) {
    double lead_distance_m = fabs(2*speed) * SG_FEET_TO_METER;
    setLeadDistance(lead_distance_m * SG_METER_TO_FEET);
    if (lead_distance_ft > 1000) {
        SG_LOG(SG_AI, SG_BULK, "Excessive leaddistance leadin 0 " << lead_distance_ft << " leadInAngle " << leadInAngle << " inbound " << inbound << " outbound " << outbound);
    }
  } else {
    double lead_distance_m = turn_radius_m * tan((leadInAngle * SG_DEGREES_TO_RADIANS)/2);
    setLeadDistance(lead_distance_m * SG_METER_TO_FEET);
    SG_LOG(SG_AI, SG_BULK, "Setting Leaddistance " << (lead_distance_ft*SG_FEET_TO_METER) << " Turnradius " << turn_radius_m << " Speed " << speed_mps << " Half turn Angle " << (leadInAngle)/2);
    if (lead_distance_ft > 1000) {
        SG_LOG(SG_AI, SG_BULK, "Excessive leaddistance possible direction change " << lead_distance_ft << " leadInAngle " << leadInAngle << " inbound " << inbound << " outbound " << outbound << " at " << current->getName());
    }
  }

  /*
  if ((lead_distance_ft > (3*turn_radius)) && (current->on_ground == false)) {
      SG_LOG(SG_AI, SG_ALERT, "Warning: Lead-in distance is large. Inbound = " << inbound
            << ". Outbound = " << outbound << ". Lead in angle = " << leadInAngle  << ". Turn radius = " << turn_radius);
       lead_distance_ft = 3 * turn_radius;
       return;
  }
  if ((leadInAngle > 90) && (current->on_ground == true)) {
      lead_distance_ft = turn_radius * tan((90 * SG_DEGREES_TO_RADIANS)/2);
      return;
  }*/
}

void FGAIFlightPlan::setLeadDistance(double distance_ft){
  lead_distance_ft = distance_ft;
  if (lead_distance_ft>10000) {
    SG_LOG(SG_AI, SG_BULK, "Excessive Leaddistance " << distance_ft);
  }
}


double FGAIFlightPlan::getBearing(FGAIWaypoint* first, FGAIWaypoint* second) const
{
  return SGGeodesy::courseDeg(first->getPos(), second->getPos());
}

double FGAIFlightPlan::getBearing(const SGGeod& aPos, FGAIWaypoint* wp) const
{
  return SGGeodesy::courseDeg(aPos, wp->getPos());
}

void FGAIFlightPlan::deleteWaypoints()
{
  for (wpt_vector_iterator i = waypoints.begin(); i != waypoints.end(); ++i)
    delete (*i);
  waypoints.clear();
  wpt_iterator = waypoints.begin();
}

// Delete all waypoints except the last,
// which we will recycle as the first waypoint in the next leg;
void FGAIFlightPlan::resetWaypoints()
{
  if (waypoints.begin() == waypoints.end())
    return;


  FGAIWaypoint* wpt = new FGAIWaypoint;
  wpt_vector_iterator i = waypoints.end();
  --i;
  wpt->setName((*i)->getName());
  wpt->setPos((*i)->getPos());
  wpt->setCrossat((*i)->getCrossat());
  wpt->setGear_down((*i)->getGear_down());
  wpt->setFlaps((*i)->getFlaps());
  wpt->setSpoilers((*i)->getSpoilers());
  wpt->setSpeedBrakes((*i)->getSpeedBrakes());
  wpt->setBeaconLight((*i)->getBeaconLight());
  wpt->setLandingLight((*i)->getLandingLight());
  wpt->setNavLight((*i)->getNavLight());
  wpt->setStrobeLight((*i)->getStrobeLight());
  wpt->setTaxiLight((*i)->getTaxiLight());
  wpt->setFinished(false);
  wpt->setOn_ground((*i)->getOn_ground());
  SG_LOG(SG_AI, SG_DEBUG, "Recycling waypoint " << wpt->getName());
  deleteWaypoints();
  pushBackWaypoint(wpt);
}

void FGAIFlightPlan::addWaypoint(FGAIWaypoint* wpt)
{
    pushBackWaypoint(wpt);
}

void FGAIFlightPlan::pushBackWaypoint(FGAIWaypoint *wpt)
{
  if (!wpt) {
    SG_LOG(SG_AI, SG_WARN, "Null WPT added");
  }
  size_t pos = wpt_iterator - waypoints.begin();
  if (waypoints.size()>0) {
      double dist = SGGeodesy::distanceM( waypoints.back()->getPos(), wpt->getPos());
      if( dist == 0 ) {
        SG_LOG(SG_AI, SG_DEBUG, "Double WP : \t" << wpt->getName() << " not added ");
      } else {
        waypoints.push_back(wpt);
        SG_LOG(SG_AI, SG_BULK, "Added WP : \t" << setprecision(12) << wpt->getName() << "\t" << wpt->getPos() << "\t" << wpt->getSpeed());
      }
  } else {
    waypoints.push_back(wpt);
    SG_LOG(SG_AI, SG_BULK, "Added WP : \t" << setprecision(12) << wpt->getName() << "\t" << wpt->getPos() << "\t" << wpt->getSpeed());
  }
  // std::vector::push_back invalidates waypoints
  //  so we should restore wpt_iterator after push_back
  //  (or it could be an index in the vector)
  wpt_iterator = waypoints.begin() + pos;
}

// Start flightplan over from the beginning
void FGAIFlightPlan::restart()
{
  wpt_iterator = waypoints.begin();
}

int FGAIFlightPlan::getRouteIndex(int i) {
  if ((i > 0) && (i < (int)waypoints.size())) {
    return waypoints[i]->getRouteIndex();
  }
  else
    return 0;
}

double FGAIFlightPlan::checkTrackLength(const string& wptName) const {
    // skip the first two waypoints: first one is behind, second one is partially done;
    double trackDistance = 0;
    wpt_vector_iterator wptvec = waypoints.begin();
    ++wptvec;
    ++wptvec;
    while ((wptvec != waypoints.end())) {
      if (*wptvec!=nullptr && (!((*wptvec)->contains(wptName)))) {
        break;
      }
      trackDistance += (*wptvec)->getTrackLength();
      ++wptvec;
    }
    if (wptvec == waypoints.end()) {
        trackDistance = 0; // name not found
    }
    return trackDistance;
}

void FGAIFlightPlan::shortenToFirst(unsigned int number, string name)
{
    while (waypoints.size() > number + 3) {
        eraseLastWaypoint();
    }
    (waypoints.back())->setName((waypoints.back())->getName() + name);
}

void FGAIFlightPlan::setGate(const ParkingAssignment& pka)
{
  gate = pka;
}

FGParking* FGAIFlightPlan::getParkingGate()
{
  return gate.parking();
}

FGAirportRef FGAIFlightPlan::departureAirport() const
{
    return departure;
}

FGAirportRef FGAIFlightPlan::arrivalAirport() const
{
    return arrival;
}

FGAIFlightPlan* FGAIFlightPlan::createDummyUserPlan()
{
    FGAIFlightPlan* fp = new FGAIFlightPlan;
    fp->isValid = false;
    return fp;
}

bool FGAIFlightPlan::empty() const
{
    return waypoints.empty();
}
