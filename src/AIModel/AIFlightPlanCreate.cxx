/******************************************************************************
 * AIFlightPlanCreate.cxx
 * Written by Durk Talsma, started May, 2004.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <algorithm>

#include <cstdlib>
#include <cstdio>

#include "AIFlightPlan.hxx"
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Airports/airport.hxx>
#include <Airports/runways.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/groundnetwork.hxx>

#include "AIAircraft.hxx"
#include "performancedata.hxx"
#include "VectorMath.hxx"

#include <Main/fg_props.hxx>
#include <Environment/environment_mgr.hxx>
#include <Environment/environment.hxx>
#include <FDM/LaRCsim/basic_aero.h>
#include <Navaids/navrecord.hxx>
#include <Traffic/Schedule.hxx>

using std::string;

/* FGAIFlightPlan::create()
 * dynamically create a flight plan for AI traffic, based on data provided by the
 * Traffic Manager, when reading a filed flightplan fails. (DT, 2004/07/10)
 *
 * This is the top-level function, and the only one that is publicly available.
 *
 */


// Check lat/lon values during initialization;
bool FGAIFlightPlan::create(FGAIAircraft * ac, FGAirport * dep,
                            FGAirport * arr, int legNr, double alt,
                            double speed, double latitude,
                            double longitude, bool firstFlight,
                            double radius, const string & fltType,
                            const string & aircraftType,
                            const string & airline, double distance)
{
    if( legNr <= AILeg::TAKEOFF )
        SG_LOG(SG_AI, SG_BULK, "Create Leg " << legNr << " " << (firstFlight?"First":"") << " Old Leg " << getLeg() << " At Airport : " << dep->getId());
    else if( legNr<= AILeg::APPROACH )
        SG_LOG(SG_AI, SG_BULK, "Create Leg " << legNr << " " << (firstFlight?"First":"") << " Old Leg " << getLeg() << " Departure Airport : " << dep->getId()  << " Arrival Airport : " << arr->getId());
    else
        SG_LOG(SG_AI, SG_BULK, "Create Leg " << legNr << " " << (firstFlight?"First":"") << " Old Leg " << getLeg() << " At Airport : " << arr->getId());

    bool retVal = true;
    int currWpt = wpt_iterator - waypoints.begin();
    switch (legNr) {
    case AILeg::STARTUP_PUSHBACK:
        retVal = createPushBack(ac, firstFlight, dep,
                                radius, fltType, aircraftType, airline);
        break;
    case AILeg::TAXI:
        retVal = createTakeoffTaxi(ac, firstFlight, dep, radius, fltType,
                          aircraftType, airline);
        break;
    case AILeg::TAKEOFF:
        retVal = createTakeOff(ac, firstFlight, dep, SGGeod::fromDeg(longitude, latitude), speed, fltType);
        break;
    case AILeg::CLIMB:
        retVal = createClimb(ac, firstFlight, dep, arr, speed, alt, fltType);
        break;
    case AILeg::CRUISE:
        retVal = createCruise(ac, firstFlight, dep, arr, SGGeod::fromDeg(longitude, latitude), speed,
                     alt, fltType);
        break;
    case AILeg::APPROACH:
        retVal = createDescent(ac, arr, SGGeod::fromDeg(longitude, latitude), speed, alt, fltType,
                      distance);
        break;
    case AILeg::HOLD:
        retVal = createHold(ac, arr, SGGeod::fromDeg(longitude, latitude), speed, alt, fltType,
                      distance);
        break;
    case AILeg::LANDING:
        retVal = createLanding(ac, arr, fltType);
        break;
    case AILeg::PARKING_TAXI:
        retVal = createLandingTaxi(ac, arr, radius, fltType, aircraftType, airline);
        break;
    case AILeg::PARKING:
        retVal = createParking(ac, arr, radius);
        break;
    default:
        //exit(1);
        SG_LOG(SG_AI, SG_ALERT,
               "AIFlightPlan::create() attempting to create unknown leg"
               " this is probably an internal program error");
        break;
    }
    //don't  increment leg right away, but only once we pass the actual last waypoint that was created.
    // to do so, mark the last waypoint with a special status flag
    if (retVal) {
        if (waypoints.empty()) {
            SG_LOG(SG_AI, SG_WARN, ac->getCallSign() << "|AIFlightPlan::create() Leg "  << legNr <<
                " created empty waypoints." );
            return retVal;
        }
        wpt_iterator = waypoints.begin() + currWpt;
        if (waypoints.back()->getName().size()==0) {
            SG_LOG(SG_AI, SG_WARN, ac->getCallSign() <<
                " Empty wpt name");

        }
        waypoints.back()->setName( waypoints.back()->getName() + string("legend"));
        // "It's pronounced Leg-end" (Roger Glover (Deep Purple): come Hell or High Water DvD, 1993)
    }
    return retVal;
}

FGAIWaypoint * FGAIFlightPlan::createOnGround(FGAIAircraft * ac,
                                   const std::string & aName,
                                   const SGGeod & aPos, double aElev,
                                   double aSpeed)
{
    FGAIWaypoint *wpt  = new FGAIWaypoint;
    wpt->setName         (aName                  );
    wpt->setLongitude    (aPos.getLongitudeDeg() );
    wpt->setLatitude     (aPos.getLatitudeDeg()  );
    wpt->setAltitude     (aElev                  );
    wpt->setSpeed        (aSpeed                 );
    wpt->setCrossat      (-10000.1               );
    wpt->setGear_down    (true                   );
    wpt->setFlaps        (0.0f                   );
    wpt->setSpoilers     (0.0f                   );
    wpt->setSpeedBrakes  (0.0f                   );
    wpt->setFinished     (false                  );
    wpt->setOn_ground    (true                   );
    wpt->setRouteIndex   (0                      );
    if (aSpeed > 0.0f) {
        wpt->setGroundLights();
    } else {
        wpt->setPowerDownLights();
    }
    return wpt;
}

FGAIWaypoint * FGAIFlightPlan::createOnRunway(FGAIAircraft * ac,
                                   const std::string & aName,
                                   const SGGeod & aPos, double aElev,
                                   double aSpeed)
{
    FGAIWaypoint * wpt = createOnGround(ac, aName, aPos, aElev, aSpeed);
    wpt->setTakeOffLights();
    return wpt;
}

void FGAIFlightPlan::createArc(FGAIAircraft *ac, const SGGeod& center, int startAngle,
int endAngle, int increment, int radius, double aElev, double altDiff, double aSpeed, const char* pattern) {
    double trackSegmentLength = (2 * M_PI * radius) / 360.0;
    double dummyAz2;
    char buffer[20];
    if (endAngle > startAngle && increment<0) {
        endAngle -= 360;
    }
    if (endAngle < startAngle && increment>0) {
        endAngle += 360;
    }

    int nPoints = fabs(fabs(endAngle - startAngle)/increment);
    double currentAltitude = aElev;
    double altDecrement = altDiff / nPoints;

    for (int i = startAngle; !(endAngle <= i && i < endAngle + fabs(increment)); i += increment) {
        if(fabs(i)>720) {
            SG_LOG(SG_AI, SG_WARN, "FGAIFlightPlan::createArc runaway " << startAngle << " " << endAngle << " " << increment);
            break;
        }
        SGGeod result;
        SGGeodesy::direct(center, i,
                        radius, result, dummyAz2);
        snprintf(buffer, sizeof(buffer), pattern, i);
        currentAltitude -= altDecrement;
        FGAIWaypoint *wpt = createInAir(ac, buffer, result, currentAltitude, aSpeed);
        wpt->setCrossat(currentAltitude);
        wpt->setTrackLength(trackSegmentLength);
        pushBackWaypoint(wpt);
    }
}

void FGAIFlightPlan::createLine( FGAIAircraft *ac, const SGGeod& startPoint, double azimuth, double dist, double aElev, double dAlt, double vDescent, const char* pattern) {
    SGGeod dummyAz2;
    double nPoints = dist/(vDescent*2);
    char buffer[20];
    double distIncrement = (dist / nPoints);

    for (int i = 1; i < nPoints; i++) {
        double currentDist = i * distIncrement;
        double currentAltitude = aElev - (i * (dAlt / nPoints));
        SGGeod result = SGGeodesy::direct(startPoint, azimuth, currentDist);
        snprintf(buffer, sizeof(buffer), pattern, i);
        FGAIWaypoint* wpt = createInAir(ac, buffer, result, currentAltitude, vDescent);
        wpt->setCrossat(currentAltitude);
        wpt->setTrackLength((dist / nPoints));
        pushBackWaypoint(wpt);
    }

}

FGAIWaypoint * FGAIFlightPlan::createInAir(FGAIAircraft * ac,
                                const std::string & aName,
                                const SGGeod & aPos, double aElev,
                                double aSpeed)
{
    FGAIWaypoint * wpt = createOnGround(ac, aName, aPos, aElev, aSpeed);
    wpt->setGear_down  (false                   );
    wpt->setFlaps      (0.0f                    );
    wpt->setSpoilers   (0.0f                    );
    wpt->setSpeedBrakes(0.0f                    );
    wpt->setOn_ground  (false                   );
    wpt->setCrossat    (aElev                   );

    if (aPos.getElevationFt() < 10000.0f) {
        wpt->setApproachLights();
    } else {
        wpt->setCruiseLights();
    }
    return wpt;
}

FGAIWaypoint * FGAIFlightPlan::clone(FGAIWaypoint * aWpt)
{
    FGAIWaypoint *wpt = new FGAIWaypoint;
    wpt->setName       ( aWpt->getName ()      );
    wpt->setLongitude  ( aWpt->getLongitude()  );
    wpt->setLatitude   ( aWpt->getLatitude()   );
    wpt->setAltitude   ( aWpt->getAltitude()   );
    wpt->setSpeed      ( aWpt->getSpeed()      );
    wpt->setCrossat    ( aWpt->getCrossat()    );
    wpt->setGear_down  ( aWpt->getGear_down()  );
    wpt->setFlaps      ( aWpt->getFlaps()      );
    wpt->setFinished   ( aWpt->isFinished()    );
    wpt->setOn_ground  ( aWpt->getOn_ground()  );
    wpt->setLandingLight (aWpt->getLandingLight() );
    wpt->setNavLight     (aWpt->getNavLight()  );
    wpt->setStrobeLight  (aWpt->getStrobeLight() );
    wpt->setTaxiLight    (aWpt->getTaxiLight() );
    wpt->setRouteIndex ( 0                     );

    return wpt;
}


FGAIWaypoint * FGAIFlightPlan::cloneWithPos(FGAIAircraft * ac, FGAIWaypoint * aWpt,
                                 const std::string & aName,
                                 const SGGeod & aPos)
{
    FGAIWaypoint *wpt = clone(aWpt);
    wpt->setName       ( aName                   );
    wpt->setLongitude  ( aPos.getLongitudeDeg () );
    wpt->setLatitude   ( aPos.getLatitudeDeg  () );

    return wpt;
}


void FGAIFlightPlan::createDefaultTakeoffTaxi(FGAIAircraft * ac,
                                              FGAirport * aAirport,
                                              FGRunway * aRunway)
{
    SGGeod runwayTakeoff = aRunway->pointOnCenterline(5.0);
    double airportElev = aAirport->getElevation();

    FGAIWaypoint *wpt;
    wpt =
        createOnGround(ac, "Airport Center", aAirport->geod(), airportElev,
                       ac->getPerformance()->vTaxi());
    pushBackWaypoint(wpt);
    wpt =
        createOnRunway(ac, "Runway Takeoff", runwayTakeoff, airportElev,
                       ac->getPerformance()->vTaxi());
    wpt->setFlaps(0.5f);
    pushBackWaypoint(wpt);


    // Acceleration point, 105 meters into the runway,
    SGGeod accelPoint = aRunway->pointOnCenterline(105.0);
    wpt = createOnRunway(ac, "Accel", accelPoint, airportElev,
                         ac->getPerformance()->vRotate());
    pushBackWaypoint(wpt);
}

bool FGAIFlightPlan::createTakeoffTaxi(FGAIAircraft * ac, bool firstFlight,
                                       FGAirport * apt,
                                       double radius,
                                       const string & fltType,
                                       const string & acType,
                                       const string & airline)
{
    int route;
    // If this function is called during initialization,
    // make sure we obtain a valid gate ID first
    // and place the model at the location of the gate.
    if (firstFlight && apt->getDynamics()->hasParkings()) {
      gate =  apt->getDynamics()->getAvailableParking(radius, fltType,
                                                        acType, airline);
      if (!gate.isValid()) {
        SG_LOG(SG_AI, SG_WARN, "Could not find parking for a " <<
               acType <<
               " of flight type " << fltType <<
               " of airline     " << airline <<
               " at airport     " << apt->getId());
      }
    }

    const string& rwyClass = getRunwayClassFromTrafficType(fltType);

    // Only set this if it hasn't been set by ATC already.
    if (activeRunway.empty()) {
        //cerr << "Getting runway for " << ac->getTrafficRef()->getCallSign() << " at " << apt->getId() << endl;
        double depHeading = ac->getTrafficRef()->getCourse();
        apt->getDynamics()->getActiveRunway(rwyClass, 1, activeRunway,
                                            depHeading);
    }
    FGRunway * rwy = apt->getRunwayByIdent(activeRunway);
    SG_LOG(SG_AI, SG_BULK, "Taxi to " << apt->getId() << "/" << activeRunway);
    assert( rwy != NULL );
    SGGeod runwayTakeoff = rwy->pointOnCenterlineDisplaced(5.0);

    FGGroundNetwork *gn = apt->groundNetwork();
    if (!gn->exists()) {
        SG_LOG(SG_AI, SG_DEBUG, "No groundnet " << apt->getId() << " creating default taxi.");
        createDefaultTakeoffTaxi(ac, apt, rwy);
        return true;
    }

    FGTaxiNodeRef runwayNode;
    if (gn->getVersion() > 0) {
        runwayNode = gn->findNearestNodeOnRunwayEntry(runwayTakeoff);
    } else {
        runwayNode = gn->findNearestNode(runwayTakeoff);
    }

    // A negative gateId indicates an overflow parking, use a
    // fallback mechanism for this.
    // Starting from gate 0 in this case is a bit of a hack
    // which requires a more proper solution later on.
  //  delete taxiRoute;
  //  taxiRoute = new FGTaxiRoute;

    // Determine which node to start from.
    FGTaxiNodeRef node;
    // Find out which node to start from
    FGParking *park = gate.parking();
    if (park) {
        node = park->getPushBackPoint();
        if (node == 0) {
            // Handle case where parking doesn't have a node
            if (firstFlight) {
                node = park;
            } else if (lastNodeVisited) {
                node = lastNodeVisited;
            } else {
                SG_LOG(SG_AI, SG_WARN, "Taxiroute could not be constructed no lastNodeVisited at " << (apt?apt->getId():"????") << gate.isValid());
            }
        }
    } else {
        SG_LOG(SG_AI, SG_WARN, "Taxiroute could not be constructed no parking." << (apt?apt->getId():"????"));
    }

    FGTaxiRoute taxiRoute;
    if (runwayNode && node) {
        taxiRoute = gn->findShortestRoute(node, runwayNode);
    } else {
    }

    // This may happen with buggy ground networks
    if (taxiRoute.size() <= 1) {
        SG_LOG(SG_AI, SG_DEBUG, "Taxiroute too short " << apt->getId() << "creating default taxi.");
        createDefaultTakeoffTaxi(ac, apt, rwy);
        return true;
    }

    taxiRoute.first();
    FGTaxiNodeRef skipNode;

    //bool isPushBackPoint = false;
    if (firstFlight) {
        // If this is called during initialization, randomly
        // skip a number of waypoints to get a more realistic
        // taxi situation.
        int nrWaypointsToSkip = rand() % taxiRoute.size();
        // but make sure we always keep two active waypoints
        // to prevent a segmentation fault
        for (int i = 0; i < nrWaypointsToSkip - 3; i++) {
            taxiRoute.next(skipNode, &route);
        }

        gate.release(); // free up our gate as required
    } else {
        if (taxiRoute.size() > 1) {
            taxiRoute.next(skipNode, &route);     // chop off the first waypoint, because that is already the last of the pushback route
        }
    }

    // push each node on the taxi route as a waypoint

    //cerr << "Building taxi route" << endl;

    // Note that the line wpt->setRouteIndex was commented out by revision [afcdbd] 2012-01-01,
    // which breaks the rendering functions.
    // These can probably be generated on the fly however.
    while (taxiRoute.next(node, &route)) {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%d", node->getIndex());
        FGAIWaypoint *wpt =
            createOnGround(ac, buffer, node->geod(), apt->getElevation(),
                           ac->getPerformance()->vTaxi());
        wpt->setRouteIndex(route);
        //cerr << "Nodes left " << taxiRoute->nodesLeft() << " ";
        if (taxiRoute.nodesLeft() == 1) {
            // Note that we actually have hold points in the ground network, but this is just an initial test.
            //cerr << "Setting departurehold point: " << endl;
            wpt->setName( wpt->getName() + string("_DepartureHold"));
            wpt->setFlaps(0.5f);
            wpt->setTakeOffLights();
        }
        if (taxiRoute.nodesLeft() == 0) {
            wpt->setName(wpt->getName() + string("_Accel"));
            wpt->setTakeOffLights();
            wpt->setFlaps(0.5f);
        }
        pushBackWaypoint(wpt);
    }
    double accell_point = 105.0;
    // Acceleration point, 105 meters into the runway,
    SGGeod entryPoint = waypoints.back()->getPos();
    SGGeod runwayEnd = rwy->pointOnCenterlineDisplaced(0);
    double distM = SGGeodesy::distanceM(entryPoint, runwayEnd);
    if (distM > accell_point) {
        SG_LOG(SG_AI, SG_BULK, "Distance down runway " << distM << " " << accell_point);
        accell_point += distM;
    }
    SGGeod accelPoint = rwy->pointOnCenterlineDisplaced(accell_point);
    FGAIWaypoint *wpt = createOnRunway(ac, "Accel", accelPoint, apt->getElevation(), ac->getPerformance()->vRotate());
    wpt->setFlaps(0.5f);
    pushBackWaypoint(wpt);

    //cerr << "[done]" << endl;
    return true;
}

void FGAIFlightPlan::createDefaultLandingTaxi(FGAIAircraft * ac,
                                              FGAirport * aAirport)
{
    SGGeod lastWptPos =
        SGGeod::fromDeg(waypoints.back()->getLongitude(),
                        waypoints.back()->getLatitude());
    double airportElev = aAirport->getElevation();

    FGAIWaypoint *wpt;
    wpt =
        createOnGround(ac, "Runway Exit", lastWptPos, airportElev,
                       ac->getPerformance()->vTaxi());
    pushBackWaypoint(wpt);
    wpt =
        createOnGround(ac, "Airport Center", aAirport->geod(), airportElev,
                       ac->getPerformance()->vTaxi());
    pushBackWaypoint(wpt);

    if (gate.isValid()) {
        wpt = createOnGround(ac, "END-taxi", gate.parking()->geod(), airportElev,
                         ac->getPerformance()->vTaxi());
        pushBackWaypoint(wpt);
    }
}

bool FGAIFlightPlan::createLandingTaxi(FGAIAircraft * ac,
                                       FGAirport * apt,
                                       double radius,
                                       const string & fltType,
                                       const string & acType,
                                       const string & airline)
{
    int route;
    gate = apt->getDynamics()->getAvailableParking(radius, fltType,
                                            acType, airline);
    SGGeod lastWptPos = waypoints.back()->getPos();
    FGGroundNetwork *gn = apt->groundNetwork();

    // Find a route from runway end to parking/gate.
    if (!gn->exists()) {
        createDefaultLandingTaxi(ac, apt);
        return true;
    }

    FGRunway * rwy = apt->getRunwayByIdent(activeRunway);
    FGTaxiNodeRef runwayNode;
    if (gn->getVersion() == 1) {
        runwayNode = gn->findNearestNodeOnRunwayExit(lastWptPos, rwy);
    } else {
        runwayNode = gn->findNearestNode(lastWptPos);
    }
    //cerr << "Using network node " << runwayId << endl;
    // A negative gateId indicates an overflow parking, use a
    // fallback mechanism for this.
    // Starting from gate 0 doesn't work, so don't try it
    FGTaxiRoute taxiRoute;
    if (runwayNode && gate.isValid()) {
        taxiRoute = gn->findShortestRoute(runwayNode, gate.parking());
    }

    if (taxiRoute.empty()) {
        createDefaultLandingTaxi(ac, apt);
        return true;
    }

    FGTaxiNodeRef node;
    taxiRoute.first();
    int size = taxiRoute.size();
    for (int i = 0; i < size; i++) {
        taxiRoute.next(node, &route);
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "landingtaxi-%d-%d",  node->getIndex(), i);
        FGAIWaypoint *wpt =
            createOnGround(ac, buffer, node->geod(), apt->getElevation(),
                           ac->getPerformance()->vTaxi());

        wpt->setRouteIndex(route);
        // next WPT must be far enough from last and far enough from parking
        if ((!waypoints.back() ||
        SGGeodesy::distanceM(waypoints.back()->getPos(), wpt->getPos()) > 1) &&
        SGGeodesy::distanceM(gate.parking()->geod(), wpt->getPos()) > 20) {
            if (waypoints.back()) {
               int dist = SGGeodesy::distanceM(waypoints.back()->getPos(), wpt->getPos());
               // pretty near better set speed down
               if (dist<10 || (size - i) < 2 ) {
                  wpt->setSpeed(ac->getPerformance()->vTaxi()/2);
               }
            }
            pushBackWaypoint(wpt);
        }
    }
    SG_LOG(SG_AI, SG_BULK, "Created taxi from " << runwayNode->getIndex() <<  " to " << gate.parking()->ident() << " at " << apt->getId());

    return true;
}

static double accelDistance(double v0, double v1, double accel)
{
  double t = fabs(v1 - v0) / accel; // time in seconds to change velocity
  // area under the v/t graph: (t * v0) + (dV / 2t) where (dV = v1 - v0)
  SG_LOG(SG_AI, SG_BULK, "Brakingtime " << t );
  return t * 0.5 * (v1 + v0);
}

// find the horizontal distance to gain the specific altiude, holding
// a constant pitch angle. Used to compute distance based on standard FD/AP
// PITCH mode prior to VS or CLIMB engaging. Visually, we want to avoid
// a dip in the nose angle after rotation, during initial climb-out.
static double pitchDistance(double pitchAngleDeg, double altGainM)
{
  return altGainM / tan(pitchAngleDeg * SG_DEGREES_TO_RADIANS);
}

/*******************************************************************
 * CreateTakeOff
 * A note on units:
 *  - Speed -> knots -> nm/hour
 *  - distance along runway =-> meters
 *  - accel / decel -> is given as knots/hour, but this is highly questionable:
 *  for a jet_transport performance class, a accel / decel rate of 5 / 2 is
 *  given respectively. According to performance data.cxx, a value of kts / second seems
 *  more likely however.
 *
 ******************************************************************/
bool FGAIFlightPlan::createTakeOff(FGAIAircraft * ac,
                                   bool firstFlight,
                                   FGAirport * apt,
                                   const SGGeod& pos,
                                   double speed,
                                   const string & fltType)
{
    SG_LOG(SG_AI, SG_BULK, "createTakeOff " << apt->getId() << "/" << activeRunway);
    double accell_point = 105.0;
  // climb-out angle in degrees. could move this to the perf-db but this
  // value is pretty sane
    const double INITIAL_PITCH_ANGLE = 10.0;

    double accel = ac->getPerformance()->acceleration();
    double vTaxi = ac->getPerformance()->vTaxi();
    double vRotate = ac->getPerformance()->vRotate();
    double vTakeoff = ac->getPerformance()->vTakeoff();

    double accelMetric = accel * SG_KT_TO_MPS;
    double vTaxiMetric = vTaxi * SG_KT_TO_MPS;
    double vRotateMetric = vRotate * SG_KT_TO_MPS;
    double vTakeoffMetric = vTakeoff * SG_KT_TO_MPS;

    FGAIWaypoint *wpt;
    // Get the current active runway, based on code from David Luff
    // This should actually be unified and extended to include
    // Preferential runway use schema's
    // NOTE: DT (2009-01-18: IIRC, this is currently already the case,
    // because the getActive runway function takes care of that.
    if (firstFlight) {
        const string& rwyClass = getRunwayClassFromTrafficType(fltType);
        double heading = ac->getTrafficRef()->getCourse();
        apt->getDynamics()->getActiveRunway(rwyClass, 1, activeRunway,
                                            heading);
    }

    // this is Sentry issue FLIGHTGEAR-DS : happens after reposition,
    // likely firstFlight is false, but activeRunway is stale
    if (!apt->hasRunwayWithIdent(activeRunway)) {
        SG_LOG(SG_AI, SG_WARN, "FGAIFlightPlan::createTakeOff: invalid active runway:" << activeRunway);
        return false;
    }
    SG_LOG(SG_AI, SG_BULK, "Takeoff from airport " << apt->getId() << "/" << activeRunway);

    FGRunway * rwy = apt->getRunwayByIdent(activeRunway);
    if (!rwy)
        return false;

    double airportElev = apt->getElevation();

    if (pos.isValid()) {
       SGGeod runwayEnd = rwy->pointOnCenterlineDisplaced(0);
       double distM = SGGeodesy::distanceM(pos, runwayEnd);
       if (distM > accell_point) {
            SG_LOG(SG_AI, SG_BULK, "Distance down runway " << distM << " " << accell_point);
            accell_point += distM;
       }
    }

    // distance from the runway threshold to accelerate to rotation speed.
    double d = accelDistance(vTaxiMetric, vRotateMetric, accelMetric) + accell_point;
    SGGeod rotatePoint = rwy->pointOnCenterlineDisplaced(d);
    wpt = createOnRunway(ac, "rotate", rotatePoint, airportElev, vRotate);
    wpt->setFlaps(0.5f);
    pushBackWaypoint(wpt);

    // After rotation, we still need to accelerate to the take-off speed.
    double t  = d + accelDistance(vRotateMetric, vTakeoffMetric, accelMetric);
    SGGeod takeoffPoint = rwy->pointOnCenterlineDisplaced(t);
    wpt = createOnRunway(ac, "takeoff", takeoffPoint, airportElev, vTakeoff);
    wpt->setGear_down(true);
    wpt->setFlaps(0.5f);
    pushBackWaypoint(wpt);

    double vRef = vTakeoff + 20; // climb-out at v2 + 20kts

    // We want gear-up to take place at ~400ft AGL.  However, the flightplan
    // will move onto the next leg once it gets within 2xspeed of the next waypoint.
    // With closely spaced waypoints on climb-out this can occur almost immediately,
    // so we put the waypoint further away.
    double gearUpDist = t + 2*vRef*SG_FEET_TO_METER + pitchDistance(INITIAL_PITCH_ANGLE, 400 * SG_FEET_TO_METER);
    SGGeod gearUpPoint = rwy->pointOnCenterlineDisplaced(gearUpDist);
    wpt = createInAir(ac, "gear-up", gearUpPoint, airportElev + 400, vRef);
    wpt->setFlaps(0.5f);
    pushBackWaypoint(wpt);

    // limit climbout speed to 240kts below 10000'
    double vClimbBelow10000 = std::min(240.0, ac->getPerformance()->vClimb());

    // create two climb-out points. This is important becuase the first climb point will
    // be a (sometimes large) turn towards the destination, and we don't want to
    // commence that turn below 2000'
    double climbOut = t + 2*vClimbBelow10000*SG_FEET_TO_METER + pitchDistance(INITIAL_PITCH_ANGLE, 2000 * SG_FEET_TO_METER);
    SGGeod climbOutPoint = rwy->pointOnCenterlineDisplaced(climbOut);
    wpt = createInAir(ac, "2000'", climbOutPoint, airportElev + 2000, vClimbBelow10000);
    pushBackWaypoint(wpt);

    climbOut = t + 2*vClimbBelow10000*SG_FEET_TO_METER + pitchDistance(INITIAL_PITCH_ANGLE, 2500 * SG_FEET_TO_METER);
    SGGeod climbOutPoint2 = rwy->pointOnCenterlineDisplaced(climbOut);
    wpt = createInAir(ac, "2500'", climbOutPoint2, airportElev + 2500, vClimbBelow10000);
    pushBackWaypoint(wpt);

    return true;
}

/*******************************************************************
 * CreateClimb
 * initialize the Aircraft in a climb.
 ******************************************************************/
bool FGAIFlightPlan::createClimb(FGAIAircraft * ac, bool firstFlight,
                                 FGAirport * apt, FGAirport* arrival,
                                 double speed, double alt,
                                 const string & fltType)
{
    double vClimb = ac->getPerformance()->vClimb();

    if (firstFlight) {
        const string& rwyClass = getRunwayClassFromTrafficType(fltType);
        double heading = ac->getTrafficRef()->getCourse();
        apt->getDynamics()->getActiveRunway(rwyClass, 1, activeRunway,
                                            heading);
    }

    if (sid) {
        for (wpt_vector_iterator i = sid->getFirstWayPoint(); i != sid->getLastWayPoint(); ++i) {
            pushBackWaypoint(clone(*(i)));
        }
    } else {
        if (!apt->hasRunwayWithIdent(activeRunway))
            return false;

        FGRunwayRef runway = apt->getRunwayByIdent(activeRunway);
        SGGeod cur = runway->end();
        if (!waypoints.empty()) {
          cur = waypoints.back()->getPos();
        }

      // compute course towards destination
        double course = SGGeodesy::courseDeg(cur, arrival->geod());
        double distance = SGGeodesy::distanceM(cur, arrival->geod());

        const double headingDiffRunway = SGMiscd::normalizePeriodic(-180, 180, course - runway->headingDeg());

        if( fabs(headingDiffRunway) <10 ) {
            SGGeod climb1 = SGGeodesy::direct(cur, course, 10 * SG_NM_TO_METER);
            FGAIWaypoint *wpt = createInAir(ac, "10000ft climb", climb1, 10000, vClimb);
            pushBackWaypoint(wpt);

            SGGeod climb2 = SGGeodesy::direct(cur, course, 20 * SG_NM_TO_METER);
            wpt = createInAir(ac, "18000ft climb", climb2, 18000, vClimb);
            pushBackWaypoint(wpt);
        } else {
            double initialTurnRadius = getTurnRadius(vClimb, true);
            double lateralOffset = initialTurnRadius;
            if (headingDiffRunway > 0.0) {
                lateralOffset *= -1.0;
            }
            SGGeod climb1 = SGGeodesy::direct(cur, runway->headingDeg(), 5 * SG_NM_TO_METER);
            FGAIWaypoint *wpt = createInAir(ac, "5000ft climb", climb1, 5000, vClimb);
            pushBackWaypoint(wpt);
            int rightAngle = headingDiffRunway>0?90:-90;
            int firstTurnIncrement = headingDiffRunway>0?2:-2;

            SGGeod firstTurnCenter = SGGeodesy::direct(climb1, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
            createArc(ac, firstTurnCenter, ac->_getHeading()-rightAngle, course-rightAngle, firstTurnIncrement, initialTurnRadius, 5000, 100, vClimb, "climb-out%03d");
            SGGeod climb2 = SGGeodesy::direct(cur, course, 20 * SG_NM_TO_METER);
            wpt = createInAir(ac, "18000ft climb", waypoints.back()->getPos(), 18000, vClimb);
            pushBackWaypoint(wpt);
        }

    }

    return true;
}

/*******************************************************************
 * CreateDescent
 * Generate a flight path from the last waypoint of the cruise to
 * the permission to land point
 ******************************************************************/
bool FGAIFlightPlan::createDescent(FGAIAircraft * ac,
                                   FGAirport * apt,
                                   const SGGeod& current,
                                   double speed,
                                   double alt,
                                   const string & fltType,
                                   double requiredDistance)
{
    // The descent path contains the following phases:
    // 1) a semi circle towards runway end
    // 2) a linear glide path from the initial position to
    // 3) a semi circle turn to final
    // 4) approach
    bool reposition = false;
    FGAIWaypoint *wpt;
    double vDescent = ac->getPerformance()->vDescent();
    double vApproach = ac->getPerformance()->vApproach();

    //Beginning of Descent
    const string& rwyClass = getRunwayClassFromTrafficType(fltType);
    double heading = ac->getTrueHeadingDeg();
    apt->getDynamics()->getActiveRunway(rwyClass, 2, activeRunway,
                                        heading);
    if (!apt->hasRunwayWithIdent(activeRunway)) {
        SG_LOG(SG_AI, SG_WARN, ac->getCallSign() <<
                           "| FGAIFlightPlan::createDescent: No such runway " << activeRunway << " at " << apt->ident());
        return false;
    }

    FGRunwayRef rwy = apt->getRunwayByIdent(activeRunway);
    if (waypoints.size()==0 && ac->getTrueHeadingDeg()==0) {
        // we obviously have no previous state and are free to position the aircraft
        double courseTowardsThreshold = SGGeodesy::courseDeg(current, rwy->pointOnCenterlineDisplaced(0));
        ac->setHeading(courseTowardsThreshold);
    }

    SGGeod threshold = rwy->threshold();
    double currElev = threshold.getElevationFt();
    double altDiff = alt - currElev - 2000;

    // depending on entry we differ approach (teardrop/direct/parallel)

    double initialTurnRadius = getTurnRadius(vDescent, true);
    //double finalTurnRadius = getTurnRadius(vApproach, true);

// get length of the downwind leg for the intended runway
    double distanceOut = apt->getDynamics()->getApproachController()->getRunway(rwy->name())->getApproachDistance();    //12 * SG_NM_TO_METER;
    //time_t previousArrivalTime=  apt->getDynamics()->getApproachController()->getRunway(rwy->name())->getEstApproachTime();

    // tells us the direction we have to turn
    const double headingDiffRunway = SGMiscd::normalizePeriodic(-180, 180, ac->getTrueHeadingDeg() - rwy->headingDeg());
    double lateralOffset = initialTurnRadius;
    if (headingDiffRunway > 0.0) {
        lateralOffset *= -1.0;
    }

    SGGeod initialTarget = rwy->pointOnCenterline(-distanceOut);
    SGGeod otherRwyEnd = rwy->pointOnCenterline(rwy->lengthM());
    SGGeod secondaryTarget =
        rwy->pointOffCenterline(-2 * distanceOut, lateralOffset);
    SGGeod secondHoldCenter =
        rwy->pointOffCenterline(-3 * distanceOut, lateralOffset);
    SGGeod refPoint = rwy->pointOnCenterline(0);
    double distance = SGGeodesy::distanceM(current, initialTarget);
    double azimuth = SGGeodesy::courseDeg(current, initialTarget);
    double secondaryAzimuth = SGGeodesy::courseDeg(current, secondaryTarget);
    double initialHeadingDiff = SGMiscd::normalizePeriodic(-180, 180, azimuth-secondaryAzimuth);
    double dummyAz2;
    double courseTowardsThreshold = SGGeodesy::courseDeg(current, rwy->pointOnCenterlineDisplaced(0));
    double courseTowardsRwyEnd = SGGeodesy::courseDeg(current, otherRwyEnd);
    double headingDiffToRunwayThreshold = SGMiscd::normalizePeriodic(-180, 180, ac->getTrueHeadingDeg() - courseTowardsThreshold);
    double headingDiffToRunwayEnd = SGMiscd::normalizePeriodic(-180, 180, ac->getTrueHeadingDeg() - courseTowardsRwyEnd);

    SG_LOG(SG_AI, SG_BULK, ac->getCallSign() <<
                           "| " <<
                           " WPs : " << waypoints.size() <<
                           " Heading Diff (rwy) : " << headingDiffRunway <<
                           " Distance : " << distance <<
                           " Azimuth : " << azimuth <<
                           " Heading : " << ac->getTrueHeadingDeg() <<
                           " Initial Headingdiff " << initialHeadingDiff <<
                           " Lateral : " << lateralOffset);
    // Erase the two bogus BOD points: Note check for conflicts with scripted AI flightPlans
    IncrementWaypoint(false);
    IncrementWaypoint(false);

    if (fabs(headingDiffRunway)>=30 && fabs(headingDiffRunway)<=150 ) {
        if (distance < (2*initialTurnRadius)) {
            // Too near so we pass over and enter over other side
            SG_LOG(SG_AI, SG_BULK, ac->getCallSign() << "| Enter near S curve");
            secondaryTarget =
                rwy->pointOffCenterline(-2 * distanceOut, -lateralOffset);
            secondHoldCenter =
                rwy->pointOffCenterline(-3 * distanceOut, -lateralOffset);

            // Entering not "straight" into runway so we do a s-curve
            int rightAngle = headingDiffRunway>0?90:-90;
            int firstTurnIncrement = headingDiffRunway>0?2:-2;

            SGGeod firstTurnCenter = SGGeodesy::direct(current, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
            SGGeod newCurrent = current;
            // If we are not far enough cross over
            if (abs(headingDiffToRunwayThreshold)<90) {
                newCurrent = SGGeodesy::direct(current, ac->getTrueHeadingDeg(), distance + 1000);
                firstTurnCenter = SGGeodesy::direct(newCurrent, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
                createLine(ac, current, ac->getTrueHeadingDeg(), distance + 1000, waypoints.size()>0?waypoints.back()->getAltitude():alt, 0, vDescent, "move%03d");
            }
            int offset = 1000;
            while (SGGeodesy::distanceM(firstTurnCenter, secondaryTarget) < 2*initialTurnRadius) {
                newCurrent = SGGeodesy::direct(newCurrent, ac->getTrueHeadingDeg(), offset+=100);
                firstTurnCenter = SGGeodesy::direct(newCurrent, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
            }
            const double heading = VectorMath::outerTangentsAngle(firstTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius )[0];
            createArc(ac, firstTurnCenter, ac->_getHeading()-rightAngle, heading-rightAngle, firstTurnIncrement, initialTurnRadius, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/4, vDescent, "near-initialturn%03d");
            double length = VectorMath::innerTangentsLength(firstTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius );
            createLine(ac, waypoints.back()->getPos(), heading, length, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/2, vDescent, "descent%03d");
            int startVal = SGMiscd::normalizePeriodic(0, 360, heading-rightAngle);
            int endVal = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg()-rightAngle);
            // Turn into runway
            createArc(ac, secondaryTarget, startVal ,endVal , firstTurnIncrement, initialTurnRadius,
            waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/4, vDescent, "turn%03d");
        } else {
            SG_LOG(SG_AI, SG_BULK, ac->getCallSign() << "| Enter far S curve");
            // Entering not "straight" into runway so we do a s-curve
            int rightAngle = headingDiffRunway>0?90:-90;
            int firstTurnIncrement = headingDiffRunway>0?2:-2;
            SGGeod firstTurnCenter = SGGeodesy::direct(current, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
            int innerTangent = headingDiffRunway<0?0:1;
            int offset = 1000;
            while (SGGeodesy::distanceM(firstTurnCenter, secondaryTarget)<2*initialTurnRadius) {
               secondaryTarget = rwy->pointOffCenterline(-2 * distanceOut + (offset+=1000), lateralOffset);
            }
            const double heading = VectorMath::innerTangentsAngle(firstTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius )[innerTangent];
            createArc(ac, firstTurnCenter, ac->_getHeading()-rightAngle, heading-rightAngle, firstTurnIncrement, initialTurnRadius, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/8, vDescent, "far-initialturn%03d");
            double length = VectorMath::innerTangentsLength(firstTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius );
            createLine(ac, waypoints.back()->getPos(), heading, length, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff*0.75, vDescent, "descent%03d");
            int startVal = SGMiscd::normalizePeriodic(0, 360, heading+rightAngle);
            int endVal = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg()+rightAngle);
            // Turn into runway
            createArc(ac, secondaryTarget, startVal, endVal, firstTurnIncrement*-1, initialTurnRadius,
            waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/8, vDescent, "s-turn%03d");
        }
    } else if (fabs(headingDiffRunway)>=150 ) {
        // We are entering downwind
        if (distance < (2*initialTurnRadius)) {
            // Too near so we pass over and enter over other side
            SG_LOG(SG_AI, SG_BULK, ac->getCallSign() << "| Enter near downrunway");
            secondaryTarget =
                rwy->pointOffCenterline(-2 * distanceOut, -lateralOffset);
            secondHoldCenter =
                rwy->pointOffCenterline(-3 * distanceOut, -lateralOffset);

            // Entering not "straight" into runway so we do a s-curve
            int rightAngle = azimuth>0?90:-90;
            int firstTurnIncrement = azimuth>0?2:-2;

            SGGeod firstTurnCenter = SGGeodesy::direct(current, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
            // rwy->headingDeg()-rightAngle
            int endVal = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg()-180);
            SGGeod secondTurnCenter = SGGeodesy::direct(firstTurnCenter, endVal, 2*initialTurnRadius);
            createArc(ac, firstTurnCenter, ac->_getHeading()-rightAngle, endVal, firstTurnIncrement, initialTurnRadius, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/4, vDescent, "d-near-initialturn%03d");
            endVal = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg());
            // int endVal2 = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg()-rightAngle);
            const double endVal2 = VectorMath::outerTangentsAngle(secondTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius )[0];
            createArc(ac, secondTurnCenter, endVal, endVal2+rightAngle, -firstTurnIncrement, initialTurnRadius, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/4, vDescent, "secondturn%03d");
            //outer
            double length = VectorMath::outerTangentsLength(secondTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius );
            createLine(ac, waypoints.back()->getPos(), endVal2, length, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/4, vDescent, "descent%03d");
            endVal = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg()+rightAngle);
            // Turn into runway
            createArc(ac, secondaryTarget, endVal2+rightAngle, endVal, -firstTurnIncrement, initialTurnRadius,
            waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/4, vDescent, "turn%03d");
        } else {
            SG_LOG(SG_AI, SG_BULK, ac->getCallSign() << "| Enter far S downrunway");
            // Entering not "straight" into runway so we do a s-curve
            int rightAngle = headingDiffRunway>0?90:-90;
            int firstTurnIncrement = headingDiffRunway>0?2:-2;
            int innerTangent = headingDiffRunway<0?0:1;
            SGGeod firstTurnCenter = SGGeodesy::direct(current, ac->getTrueHeadingDeg() + rightAngle, initialTurnRadius);
            const double heading = VectorMath::innerTangentsAngle(firstTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius )[innerTangent];
            createArc(ac, firstTurnCenter, ac->_getHeading()-rightAngle, heading-rightAngle, firstTurnIncrement, initialTurnRadius, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/3, vDescent, "d-far-initialturn%03d");
            double length = VectorMath::innerTangentsLength(firstTurnCenter, secondaryTarget, initialTurnRadius, initialTurnRadius );
            createLine(ac, waypoints.back()->getPos(), heading, length, waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/3, vDescent, "descent%03d");
            int startVal = SGMiscd::normalizePeriodic(0, 360, heading+rightAngle);
            int endVal = SGMiscd::normalizePeriodic(0, 360, rwy->headingDeg()+rightAngle);
            // Turn into runway
            createArc(ac, secondaryTarget, startVal, endVal, -firstTurnIncrement, initialTurnRadius,
            waypoints.size()>0?waypoints.back()->getAltitude():alt, altDiff/3, vDescent, "d-s-turn%03d");
        }
    } else {
        SG_LOG(SG_AI, SG_BULK, ac->getCallSign() << "| Enter far straight");
        // Entering "straight" into runway so only one turn
        int rightAngle = headingDiffRunway>0?90:-90;
        SGGeod firstTurnCenter = SGGeodesy::direct(current, ac->getTrueHeadingDeg() - rightAngle, initialTurnRadius);
        int firstTurnIncrement = headingDiffRunway>0?-2:2;
        const double heading = rwy->headingDeg();
        createArc(ac, firstTurnCenter, ac->_getHeading()+rightAngle, heading+rightAngle, firstTurnIncrement, initialTurnRadius, ac->getAltitude(), altDiff/3, vDescent, "straight_turn_%03d");
    }

    time_t now = globals->get_time_params()->get_cur_time();

    arrivalTime = now;
    //choose a distance to the runway such that it will take at least 60 seconds more
    // time to get there than the previous aircraft.
    // Don't bother when aircraft need to be repositioned, because that marks the initialization phased...
    return true;
}

/*******************************************************************
 * CreateHold
 * Generate a hold flight path from the permission to land point
 * Exactly one hold pattern
 ******************************************************************/
bool FGAIFlightPlan::createHold(FGAIAircraft * ac,
                                   FGAirport * apt,
                                   const SGGeod& current,
                                   double speed,
                                   double alt,
                                   const string & fltType,
                                   double requiredDistance)
{
    //Beginning of Descent
    const string& rwyClass = getRunwayClassFromTrafficType(fltType);
    double vDescent = ac->getPerformance()->vDescent();
    double vApproach = ac->getPerformance()->vApproach();
    double initialTurnRadius = getTurnRadius(vDescent, true);
    double heading = ac->getTrueHeadingDeg();
    apt->getDynamics()->getActiveRunway(rwyClass, 2, activeRunway,
                                        heading);
    if (!apt->hasRunwayWithIdent(activeRunway)) {
        SG_LOG(SG_AI, SG_WARN, ac->getCallSign() <<
                           "| FGAIFlightPlan::createHold: No such runway " << activeRunway << " at " << apt->ident());
        return false;
    }
    FGRunwayRef rwy = apt->getRunwayByIdent(activeRunway);
    double currentAltitude = waypoints.back()->getAltitude();
    double distanceOut = apt->getDynamics()->getApproachController()->getRunway(rwy->name())->getApproachDistance();    //12 * SG_NM_TO_METER;
    double lateralOffset = initialTurnRadius;

    SGGeod secondaryTarget = rwy->pointOffCenterline(-2 * distanceOut, lateralOffset);
    SGGeod secondHoldCenter = rwy->pointOffCenterline(-4 * distanceOut, lateralOffset);

    createArc(ac, secondaryTarget, rwy->headingDeg()-90, rwy->headingDeg()+90, 5, initialTurnRadius,
    currentAltitude, 0, vDescent, "hold_1_%03d");
    createArc(ac, secondHoldCenter, rwy->headingDeg()+90, rwy->headingDeg()-90, 5, initialTurnRadius,
    currentAltitude, 0, vDescent, "hold_2_%03d");

    return true;
}
/**
 * compute the distance along the centerline, to the ILS glideslope
 * transmitter. Return -1 if there's no GS for the runway
 */
static double runwayGlideslopeTouchdownDistance(FGRunway* rwy)
{
  FGNavRecord* gs = rwy->glideslope();
  if (!gs) {
    return -1;
  }

  SGVec3d runwayPosCart = SGVec3d::fromGeod(rwy->pointOnCenterline(0.0));
  // compute a unit vector in ECF cartesian space, from the runway beginning to the end
  SGVec3d runwayDirectionVec = normalize(SGVec3d::fromGeod(rwy->end()) - runwayPosCart);
  SGVec3d gsTransmitterVec = gs->cart() - runwayPosCart;

// project the gsTransmitterVec along the runwayDirctionVec to get out
// final value (in metres)
  double dist = dot(runwayDirectionVec, gsTransmitterVec);
  return dist;
}

/*******************************************************************
 * CreateLanding (Leg 7)
 * Create a flight path from the "permision to land" point (currently
   hardcoded at 5000 meters from the threshold) to the threshold, at
   a standard glide slope angle of 3 degrees.
   Position : 50.0354 8.52592 384 364 11112
 ******************************************************************/
bool FGAIFlightPlan::createLanding(FGAIAircraft * ac, FGAirport * apt,
                                   const string & fltType)
{
    double vTouchdown = ac->getPerformance()->vTouchdown();
    double vTaxi      = ac->getPerformance()->vTaxi();
    double decel     = ac->getPerformance()->decelerationOnGround();
    double vApproach = ac->getPerformance()->vApproach();

    double vTouchdownMetric = vTouchdown  * SG_KT_TO_MPS;
    double vTaxiMetric      = vTaxi       * SG_KT_TO_MPS;
    double decelMetric      = decel       * SG_KT_TO_MPS;

    char buffer[30];
    if (!apt->hasRunwayWithIdent(activeRunway)) {
        SG_LOG(SG_AI, SG_WARN, "FGAIFlightPlan::createLanding: No such runway " << activeRunway << " at " << apt->ident());
        return false;
    }


    FGRunway * rwy = apt->getRunwayByIdent(activeRunway);
    if (!rwy) {
        return false;
    }

    // depending on entry we differ approach (teardrop/direct/parallel)
    const double headingDiff = ac->getBearing(rwy->headingDeg());
    SG_LOG(SG_AI, SG_BULK, "Heading Diff " << headingDiff);

    SGGeod threshold = rwy->threshold();
    double currElev = threshold.getElevationFt();

    double touchdownDistance = runwayGlideslopeTouchdownDistance(rwy);
    if (touchdownDistance < 0.0) {
      double landingLength = rwy->lengthM() - (rwy->displacedThresholdM());
      // touchdown 25% of the way along the landing area
      touchdownDistance = rwy->displacedThresholdM() + (landingLength * 0.25);
    }

    const double tanGlideslope = tan(3.0);

    SGGeod coord;
  // find glideslope entry point, 2000' above touchdown elevation
    double glideslopeEntry = -((2000 * SG_FEET_TO_METER) / tanGlideslope) + touchdownDistance;

    snprintf(buffer, sizeof(buffer), "Glideslope begin Rwy %s", activeRunway.c_str());

    FGAIWaypoint *wpt = createInAir(ac, buffer, rwy->pointOnCenterline(-glideslopeEntry),
                                  currElev + 2000, vApproach);
    wpt->setGear_down(true);
    wpt->setFlaps(1.0f);
    wpt->setSpeedBrakes(1.0f);
    pushBackWaypoint(wpt);

  // deceleration point, 500' above touchdown elevation - slow from approach speed
  // to touchdown speed
    double decelPoint = -((500 * SG_FEET_TO_METER) / tanGlideslope) + touchdownDistance;
    wpt = createInAir(ac, "500' decel", rwy->pointOnCenterline(-decelPoint),
                                  currElev + 500, vTouchdown);
    wpt->setGear_down(true);
    wpt->setFlaps(1.0f);
    wpt->setSpeedBrakes(1.0f);
    pushBackWaypoint(wpt);

  // compute elevation above the runway start, based on a 3-degree glideslope
    double heightAboveRunwayStart = touchdownDistance *
      tan(3.0 * SG_DEGREES_TO_RADIANS) * SG_METER_TO_FEET;
    wpt = createInAir(ac, "CrossThreshold", rwy->begin(),
                      heightAboveRunwayStart + currElev, vTouchdown);
    wpt->setGear_down(true);
    wpt->setFlaps(1.0f);
    wpt->setSpeedBrakes(1.0f);
    pushBackWaypoint(wpt);

    double rolloutDistance = accelDistance(vTouchdownMetric, vTaxiMetric, decelMetric);

    SG_LOG(SG_AI, SG_BULK, "Landing " << glideslopeEntry << "\t" << decelPoint << " Rollout " << rolloutDistance);

    int nPoints = (int)(rolloutDistance/60);
    for (int i = 1; i <= nPoints; i++) {
        snprintf(buffer, sizeof(buffer), "rollout%03d", i);
        double t = 1-pow((double) (nPoints - i), 2) / pow(nPoints, 2);
        coord = rwy->pointOnCenterline(touchdownDistance + (rolloutDistance * t));
        double vel = (vTouchdownMetric * (1.0 - t)) + (vTaxiMetric * t);
        wpt = createOnRunway(ac, buffer, coord, currElev, vel);
        wpt->setFlaps(1.0f);
        wpt->setSpeedBrakes(1.0f);
        wpt->setSpoilers(1.0f);
        wpt->setCrossat(currElev);
        pushBackWaypoint(wpt);
    }

    wpt->setSpeed(vTaxi);
    double mindist = (1.1 * rolloutDistance) + touchdownDistance;

    FGGroundNetwork *gn = apt->groundNetwork();
    if (!gn) {
        SG_LOG(SG_AI, SG_DEBUG, "No groundnet " << apt->getId() << " no landing created.");
        return true;
    }

    coord = rwy->pointOnCenterline(mindist);
    FGTaxiNodeRef tn;
    if (gn->getVersion() > 0) {
        tn = gn->findNearestNodeOnRunwayExit(coord, rwy);
    } else {
        tn = gn->findNearestNode(coord);
    }

    if (tn) {
        wpt = createOnRunway(ac, buffer, tn->geod(), currElev, vTaxi);
        wpt->setFlaps(1.0f);
        wpt->setSpeedBrakes(1.0f);
        wpt->setSpoilers(0.0f);
        pushBackWaypoint(wpt);
    }

    return true;
}

/*******************************************************************
 * createParking Leg 9
 * initialize the Aircraft at the parking location
 ******************************************************************/
bool FGAIFlightPlan::createParking(FGAIAircraft * ac, FGAirport * apt,
                                   double radius)
{
    FGAIWaypoint *wpt;
    double aptElev = apt->getElevation();
    double vTaxi = ac->getPerformance()->vTaxi();
    double vTaxiReduced = vTaxi * (2.0 / 3.0);
    if (!gate.isValid()) {
      wpt = createOnGround(ac, "END-ParkingInvalidGate", apt->geod(), aptElev,
                           vTaxiReduced);
      pushBackWaypoint(wpt);
      return true;
    }

    FGParking* parking = gate.parking();
    double reverseHeading = SGMiscd::normalizePeriodic(0, 360, parking->getHeading() + 180.0);
    double az; // unused
    SGGeod pos;

    SGGeodesy::direct(parking->geod(), reverseHeading, 18,
                      pos, az);

    wpt = createOnGround(ac, "parking1", pos, aptElev, 3);
    pushBackWaypoint(wpt);

    SGGeodesy::direct(parking->geod(), reverseHeading, 14,
                    pos, az);
    wpt = createOnGround(ac, "parking2", pos, aptElev, 3);
    pushBackWaypoint(wpt);

    SGGeodesy::direct(parking->geod(), reverseHeading, 10,
                    pos, az);
    wpt = createOnGround(ac, "parking3", pos, aptElev, 2);
    pushBackWaypoint(wpt);

    SGGeodesy::direct(parking->geod(), reverseHeading, 6,
                    pos, az);
    wpt = createOnGround(ac, "parking4", pos, aptElev, 2);
    pushBackWaypoint(wpt);

    SGGeodesy::direct(parking->geod(), reverseHeading, 3,
                    pos, az);
    wpt = createOnGround(ac, "parking5", pos, aptElev, 2);
    pushBackWaypoint(wpt);

    char buffer[30];
    snprintf(buffer, sizeof(buffer), "Parking-%s", parking->getName().c_str());

    wpt = createOnGround(ac, buffer, parking->geod(), aptElev, vTaxiReduced/3);
    pushBackWaypoint(wpt);
    SGGeodesy::direct(parking->geod(), parking->getHeading(), 2,
                    pos, az);
    wpt = createOnGround(ac, "Beyond-Parking", pos, aptElev, vTaxiReduced/3);
    pushBackWaypoint(wpt);
    SGGeodesy::direct(parking->geod(), parking->getHeading(), 3,
                    pos, az);
    wpt = createOnGround(ac, "END-Parking", pos, aptElev, vTaxiReduced/3);
    pushBackWaypoint(wpt);
    return true;
}

/**
 *
 * @param fltType a string describing the type of
 * traffic, normally used for gate assignments
 * @return a converted string that gives the runway
 * preference schedule to be used at aircraft having
 * a preferential runway schedule implemented (i.e.
 * having a rwyprefs.xml file
 *
 * Currently valid traffic types for gate assignment:
 * - gate (commercial gate)
 * - cargo (commercial gargo),
 * - ga (general aviation) ,
 * - ul (ultralight),
 * - mil-fighter (military - fighter),
 * - mil-transport (military - transport)
 *
 * Valid runway classes:
 * - com (commercial traffic: jetliners, passenger and cargo)
 * - gen (general aviation)
 * - ul (ultralight: I can imagine that these may share a runway with ga on some airports)
 * - mil (all military traffic)
 */
const char* FGAIFlightPlan::getRunwayClassFromTrafficType(const string& fltType)
{
    if ((fltType == "gate") || (fltType == "cargo")) {
        return "com";
    }
    if (fltType == "ga") {
        return "gen";
    }
    if (fltType == "ul") {
        return "ul";
    }
    if ((fltType == "mil-fighter") || (fltType == "mil-transport")) {
        return "mil";
    }
    return "com";
}


double FGAIFlightPlan::getTurnRadius(double speed, bool inAir)
{
    double turn_radius;
    if (inAir == false) {
        turn_radius = ((360 / 30) * fabs(speed)) / (2 * M_PI);
    } else {
        turn_radius = 0.1911 * speed * speed;   // an estimate for 25 degrees bank
    }
    return turn_radius;
}
