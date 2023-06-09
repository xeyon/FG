// flight.hxx -- define shared flight model parameters
//
// Written by Curtis Olson, started May 1997.
//
// Copyright (C) 1997  Curtis L. Olson  - http://www.flightgear.org/~curt
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
//
// $Id$


#ifndef _FLIGHT_HXX
#define _FLIGHT_HXX


#ifndef __cplusplus
# error This library requires C++
#endif


/* Required get_()

   `FGInterface::get_Longitude ()'
   `FGInterface::get_Latitude ()'
   `FGInterface::get_Altitude ()'
   `FGInterface::get_Phi ()'
   `FGInterface::get_Theta ()'
   `FGInterface::get_Psi ()'
   `FGInterface::get_V_equiv_kts ()'

   `FGInterface::get_V_north ()'
   `FGInterface::get_V_east ()'
   `FGInterface::get_V_down ()'

   `FGInterface::get_P_Body ()'
   `FGInterface::get_Q_Body ()'
   `FGInterface::get_R_Body ()'

   `FGInterface::get_Gamma_vert_rad ()'
   `FGInterface::get_Climb_Rate ()'
   `FGInterface::get_Alpha ()'
   `FGInterface::get_Beta ()'

   `FGInterface::get_Runway_altitude ()'

   `FGInterface::get_Lon_geocentric ()'
   `FGInterface::get_Lat_geocentric ()'
   `FGInterface::get_Sea_level_radius ()'
   `FGInterface::get_Earth_position_angle ()'

   `FGInterface::get_Latitude_dot()'
   `FGInterface::get_Longitude_dot()'
   `FGInterface::get_Radius_dot()'

   `FGInterface::get_Dx_cg ()'
   `FGInterface::get_Dy_cg ()'
   `FGInterface::get_Dz_cg ()'

   `FGInterface::get_Radius_to_vehicle ()'

 */


#include <cmath>
#include <functional>

#include <simgear/compiler.h>
#include <simgear/constants.h>
#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/tiedpropertylist.hxx>
#include <FDM/groundcache.hxx>
#include <FDM/groundreactions.hxx>
#include <FDM/AIWake/AIWakeGroup.hxx>

namespace simgear {
class BVHMaterial;
}

class SGIOChannel;
class FGAIAircraft;

/**
 * A little helper class to update the track if
 * the position has changed. In the constructor, 
 * create a copy of the current position and store 
 * references to the position object and the track
 * variable to update.
 * The destructor, called at TrackComputer's end of 
 * life/visibility, computes the track if the 
 * position has changed.
 */
class TrackComputer
{
public:
    inline TrackComputer( double & track, double & path, const SGGeod & position ) : 
        _track( track ),
        _path( path ),
        _position( position ),
        _prevPosition( position ) {
    }

    inline ~TrackComputer() {
        if( _prevPosition == _position ) return;
//        _track = SGGeodesy::courseDeg( _prevPosition, _position );
        double d = .0;
        double distance = .0;
        if( SGGeodesy::inverse( _prevPosition, _position, _track, d, distance ) ) {
            d = _position.getElevationM() - _prevPosition.getElevationM();
            _path = atan2( d, distance ) * SGD_RADIANS_TO_DEGREES;
        }
    }

private:
    double & _track;
    double & _path;
    const SGGeod & _position;
    const SGGeod _prevPosition;
};

// This is based heavily on LaRCsim/ls_generic.h
class FGInterface : public SGSubsystem
{
    // Has the init() method been called.  This is used to delay
    // initialization until scenery can be loaded and we know the true
    // ground elevation.
    bool inited;

    // Have we bound to the property system
    bool bound;

    // periodic update management variable.  This is a scheme to run
    // the fdm with a fixed delta-t.  We control how many iteration of
    // the fdm to run with the fixed dt based on the elapsed time from
    // the last update.  This allows us to maintain sync with the real
    // time clock, even though each frame could take a random amount
    // of time.  Since "dt" is unlikely to divide evenly into the
    // elapse time, we keep track of the remainder and add it into the
    // next elapsed time.  This yields a small amount of temporal
    // jitter ( < dt ) but in practice seems to work well.

    /**
     * encapsulate primary flight state. This is packaged so it can be
     * (unfortunately) sent directly over the wire by the 'native' FDM
     * protocol.
     */
    struct FlightState
    {
        // CG position w.r.t. ref. point
        SGVec3d d_cg_rp_body_v;

        // Accelerations
        SGVec3d v_dot_local_v;
        SGVec3d v_dot_body_v;
        SGVec3d a_cg_body_v;
        SGVec3d a_pilot_body_v;
        SGVec3d n_cg_body_v;
        SGVec3d omega_dot_body_v;

        // Velocities
        SGVec3d v_local_v;
        SGVec3d v_local_rel_ground_v; // V rel w.r.t. earth surface
        SGVec3d v_local_airmass_v;    // velocity of airmass (steady winds)
        SGVec3d v_body_v;        // ECEF velocities in body axis

        SGVec3d omega_body_v;         // Angular B rates
        SGVec3d euler_rates_v;
        SGVec3d geocentric_rates_v;   // Geocentric linear velocities

        // Positions
        SGGeod geodetic_position_v;
        SGVec3d cartesian_position_v;
        SGGeoc geocentric_position_v;
        SGVec3d euler_angles_v;

        // Normal Load Factor
        double nlf;

        // Velocities
        double v_rel_wind, v_true_kts;
        double v_ground_speed, v_equiv_kts;
        double v_calibrated_kts;

        // Miscellaneious Quantities
        double alpha, beta;  // in radians
        double gamma_vert_rad;  // Flight path angles
        double density, mach_number;
        double static_pressure, total_pressure;
        double dynamic_pressure;
        double static_temperature, total_temperature;
        double sea_level_radius, earth_position_angle;
        double runway_altitude;
        double climb_rate;                // in feet per second
        double altitude_agl;
        double track;
        double path;
    };
    
    FlightState _state;
    
    simgear::TiedPropertyList _tiedProperties;

    // the ground cache object itself.
    FGGroundCache ground_cache;

    GroundReactions _groundReactions;
    AIWakeGroup wake_group;

    void set_A_X_pilot(double x)
    { _set_Accels_Pilot_Body(x, _state.a_pilot_body_v[1], _state.a_pilot_body_v[2]); }
    
    void set_A_Y_pilot(double y)
    { _set_Accels_Pilot_Body(_state.a_pilot_body_v[0], y, _state.a_pilot_body_v[2]); }
    
    void set_A_Z_pilot(double z)
    { _set_Accels_Pilot_Body(_state.a_pilot_body_v[0], _state.a_pilot_body_v[1], z); }

protected:
    int _calc_multiloop (double dt);


    // deliberately not virtual so that
    // FGInterface constructor will call
    // the right version
    void _setup();

    void _busdump(void);
    void _updatePositionM(const SGVec3d& cartPos);
    void _updatePositionFt(const SGVec3d& cartPos) {
        _updatePositionM(SG_FEET_TO_METER*cartPos);
    }
    void _updatePosition(const SGGeod& geod);
    void _updatePosition(const SGGeoc& geoc);
  
    void _updateGeodeticPosition( double lat, double lon, double alt );
    void _updateGeocentricPosition( double lat_geoc, double lon, double alt );
    void _update_ground_elev_at_pos( void );

    inline void _set_CG_Position( double dx, double dy, double dz ) {
        _state.d_cg_rp_body_v[0] = dx;
        _state.d_cg_rp_body_v[1] = dy;
        _state.d_cg_rp_body_v[2] = dz;
    }
    inline void _set_Accels_Local( double north, double east, double down ) {
        _state.v_dot_local_v[0] = north;
        _state.v_dot_local_v[1] = east;
        _state.v_dot_local_v[2] = down;
    }
    inline void _set_Accels_Body( double u, double v, double w ) {
        _state.v_dot_body_v[0] = u;
        _state.v_dot_body_v[1] = v;
        _state.v_dot_body_v[2] = w;
    }
    inline void _set_Accels_CG_Body( double x, double y, double z ) {
        _state.a_cg_body_v[0] = x;
        _state.a_cg_body_v[1] = y;
        _state.a_cg_body_v[2] = z;
    }
    inline void _set_Accels_Pilot_Body( double x, double y, double z ) {
        _state.a_pilot_body_v[0] = x;
        _state.a_pilot_body_v[1] = y;
        _state.a_pilot_body_v[2] = z;
    }
    inline void _set_Accels_CG_Body_N( double x, double y, double z ) {
        _state.n_cg_body_v[0] = x;
        _state.n_cg_body_v[1] = y;
        _state.n_cg_body_v[2] = z;
    }
    void _set_Nlf(double n) { _state.nlf=n;  }
    inline void _set_Velocities_Local( double north, double east, double down ){
        _state.v_local_v[0] = north;
        _state.v_local_v[1] = east;
        _state.v_local_v[2] = down;
    }
    inline void _set_Velocities_Ground(double north, double east, double down) {
        _state.v_local_rel_ground_v[0] = north;
        _state.v_local_rel_ground_v[1] = east;
        _state.v_local_rel_ground_v[2] = down;
    }
    inline void _set_Velocities_Local_Airmass( double north, double east, 
                                              double down)
    {
        _state.v_local_airmass_v[0] = north;
        _state.v_local_airmass_v[1] = east;
        _state.v_local_airmass_v[2] = down;
    }
    inline void _set_Velocities_Body( double u, double v, double w) {
        _state.v_body_v[0] = u;
        _state.v_body_v[1] = v;
        _state.v_body_v[2] = w;
    }
    inline void _set_V_rel_wind(double vt) { _state.v_rel_wind = vt; }
    inline void _set_V_ground_speed( double v) { _state.v_ground_speed = v; }
    inline void _set_V_equiv_kts( double kts ) { _state.v_equiv_kts = kts; }
    inline void _set_V_calibrated_kts( double kts ) { _state.v_calibrated_kts = kts; }
    inline void _set_Omega_Body( double p, double q, double r ) {
        _state.omega_body_v[0] = p;
        _state.omega_body_v[1] = q;
        _state.omega_body_v[2] = r;
    }
    inline void _set_Euler_Rates( double phi, double theta, double psi ) {
        _state.euler_rates_v[0] = phi;
        _state.euler_rates_v[1] = theta;
        _state.euler_rates_v[2] = psi;
    }
    
    void set_Phi_dot_degps(double x)
    {
        _state.euler_rates_v[0] = x * SGD_DEGREES_TO_RADIANS;
    }
    
    void set_Theta_dot_degps(double x)
    {
        _state.euler_rates_v[1] = x * SGD_DEGREES_TO_RADIANS;
    }
    
    void set_Psi_dot_degps(double x)
    {
        _state.euler_rates_v[2] = x * SGD_DEGREES_TO_RADIANS;
    }
    
    inline void _set_Geocentric_Rates( double lat, double lon, double rad ) {
        _state.geocentric_rates_v[0] = lat;
        _state.geocentric_rates_v[1] = lon;
        _state.geocentric_rates_v[2] = rad;
    }
    inline void _set_Geocentric_Position( double lat, double lon, double rad ) {
        _state.geocentric_position_v.setLatitudeRad(lat);
        _state.geocentric_position_v.setLongitudeRad(lon);
        _state.geocentric_position_v.setRadiusFt(rad);
    }
/*  Don't call _set_L[at|ong]itude() directly, use _set_Geodetic_Position() instead.
    These methods can't update the track.
 *
    inline void _set_Latitude(double lat) {
        geodetic_position_v.setLatitudeRad(lat);
    }
    inline void _set_Longitude(double lon) {
        geodetic_position_v.setLongitudeRad(lon);
    }
*/
    inline void _set_Altitude(double altitude) {
        _state.geodetic_position_v.setElevationFt(altitude);
    }
    inline void _set_Altitude_AGL(double agl) {
        _state.altitude_agl = agl;
    }
    inline void _set_Geodetic_Position( double lat, double lon ) {
        _set_Geodetic_Position( lat, lon, _state.geodetic_position_v.getElevationFt());
    }
    inline void _set_Geodetic_Position( double lat, double lon, double alt ) {
        TrackComputer tracker( _state.track, _state.path, _state.geodetic_position_v );
        _state.geodetic_position_v.setLatitudeRad(lat);
        _state.geodetic_position_v.setLongitudeRad(lon);
        _state.geodetic_position_v.setElevationFt(alt);
    }
    inline void _set_Euler_Angles( double phi, double theta, double psi ) {
        _state.euler_angles_v[0] = phi;
        _state.euler_angles_v[1] = theta;
        _state.euler_angles_v[2] = psi;
    }
    // FIXME, for compatibility with JSBSim
    inline void _set_T_Local_to_Body( int i, int j, double value) { }
    inline void _set_Alpha( double a ) { _state.alpha = a; }
    inline void _set_Beta( double b ) { _state.beta = b; }
    
    inline void set_Alpha_deg( double a ) { _state.alpha = a * SGD_DEGREES_TO_RADIANS; }
    
    inline void _set_Gamma_vert_rad( double gv ) { _state.gamma_vert_rad = gv; }
    inline void _set_Density( double d ) { _state.density = d; }
    inline void _set_Mach_number( double m ) { _state.mach_number = m; }
    inline void _set_Static_pressure( double sp ) { _state.static_pressure = sp; }
    inline void _set_Static_temperature( double t ) { _state.static_temperature = t; } 
    inline void _set_Total_temperature( double tat ) { _state.total_temperature = tat; } //JW
    inline void _set_Sea_level_radius( double r ) { _state.sea_level_radius = r; }
    inline void _set_Earth_position_angle(double a) {_state.earth_position_angle = a; }
    inline void _set_Runway_altitude( double alt ) { _state.runway_altitude = alt; }
    inline void _set_Climb_Rate(double rate) { _state.climb_rate = rate; }

public:
    FGInterface();
    FGInterface( double dt );
    virtual ~FGInterface();

    // Subsystem API.
    void bind() override;
    void init() override;
    void unbind() override;
    void update(double dt) override;

    virtual bool ToggleDataLogging(bool state) { return false; }
    virtual bool ToggleDataLogging(void) { return false; }

    bool readState(SGIOChannel* io);
    bool writeState(SGIOChannel* io);
    
    // Define the various supported flight models (many not yet implemented)
    enum {
        // Magic Carpet mode
        FG_MAGICCARPET = 0,

        // The NASA LaRCsim (Navion) flight model
        FG_LARCSIM = 1,

        // Jon S. Berndt's new FDM written from the ground up in C++
        FG_JSBSIM = 2,

        // Christian's hot air balloon simulation
        FG_BALLOONSIM = 3,

        // Aeronautical DEvelopment AGEncy, Bangalore India
        FG_ADA = 4,

        // The following aren't implemented but are here to spark
        // thoughts and discussions, and maybe even action.
        FG_ACM = 5,
        FG_SUPER_SONIC = 6,
        FG_HELICOPTER = 7,
        FG_AUTOGYRO = 8,
        FG_PARACHUTE = 9,

        // Driven externally via a serial port, net, file, etc.
        FG_EXTERNAL = 10
    };

    // initialization
    inline bool get_inited() const { return inited; }
    inline void set_inited( bool value ) { inited = value; }

    inline bool get_bound() const { return bound; }

    //perform initializion that is common to all FDM's
    void common_init();

    // Makes possibly multiple calls of fn() with pairs of property paths that
    // are associated in the FDM. Default implementation does nothing. Used by
    // the Highlight subsystem.
    virtual void property_associations(
            std::function<void(const std::string& from, const std::string& to)> fn
            );

    // Positions
    virtual void set_Latitude(double lat);       // geocentric
    virtual void set_Longitude(double lon);    
    virtual void set_Altitude(double alt);  // triggers re-calc of AGL altitude
    virtual void set_AltitudeAGL(double altagl); // and vice-versa
    virtual void set_Latitude_deg (double lat) {
        set_Latitude(lat * SGD_DEGREES_TO_RADIANS);
    }
    virtual void set_Longitude_deg (double lon) {
        set_Longitude(lon * SGD_DEGREES_TO_RADIANS);
    }
    
    // Speeds -- setting any of these will trigger a re-calc of the rest
    virtual void set_V_calibrated_kts(double vc);
    virtual void set_Mach_number(double mach);
    virtual void set_Velocities_Local( double north, double east, double down );
    inline void set_V_north (double north) { 
        set_Velocities_Local(north, _state.v_local_v[1], _state.v_local_v[2]);
    }
    inline void set_V_east (double east) { 
        set_Velocities_Local(_state.v_local_v[0], east, _state.v_local_v[2]);
    }
    inline void set_V_down (double down) { 
        set_Velocities_Local(_state.v_local_v[0], _state.v_local_v[1], down);
    }
    virtual void set_Velocities_Body( double u, double v, double w);
    virtual void set_uBody (double uBody) { 
        set_Velocities_Body(uBody, _state.v_body_v[1], _state.v_body_v[2]);
    }
    virtual void set_vBody (double vBody) { 
        set_Velocities_Body(_state.v_body_v[0], vBody, _state.v_body_v[2]);
    }
    virtual void set_wBody (double wBody) {
        set_Velocities_Body(_state.v_body_v[0], _state.v_body_v[1], wBody);
    }
    
    // Euler angles 
    virtual void set_Euler_Angles( double phi, double theta, double psi );
    virtual void set_Phi (double phi) {
        set_Euler_Angles(phi, get_Theta(), get_Psi());
    }
    virtual void set_Theta (double theta) {
        set_Euler_Angles(get_Phi(), theta, get_Psi());
    }
    virtual void set_Psi (double psi) { 
        set_Euler_Angles(get_Phi(), get_Theta(), psi);
    }
    virtual void set_Phi_deg (double phi) {
        set_Phi(phi * SGD_DEGREES_TO_RADIANS);
    }
    virtual void set_Theta_deg (double theta) {
        set_Theta(theta * SGD_DEGREES_TO_RADIANS); 
    }
    virtual void set_Psi_deg (double psi) {
        set_Psi(psi * SGD_DEGREES_TO_RADIANS);
    }
    
    // Flight Path
    virtual void set_Climb_Rate( double roc);
    virtual void set_Gamma_vert_rad( double gamma);
    
    // Earth
    
    virtual void set_Static_pressure(double p);
    virtual void set_Static_temperature(double T);
    virtual void set_Density(double rho);
    
    virtual void set_Velocities_Local_Airmass (double wnorth, 
                                               double weast, 
                                               double wdown );

    // ========== Mass properties and geometry values ==========

    // CG position w.r.t. ref. point
    inline double get_Dx_cg() const { return _state.d_cg_rp_body_v[0]; }
    inline double get_Dy_cg() const { return _state.d_cg_rp_body_v[1]; }
    inline double get_Dz_cg() const { return _state.d_cg_rp_body_v[2]; }

    // ========== Accelerations ==========

    inline double get_V_dot_north() const { return _state.v_dot_local_v[0]; }
    inline double get_V_dot_east() const { return _state.v_dot_local_v[1]; }
    inline double get_V_dot_down() const { return _state.v_dot_local_v[2]; }

    inline double get_U_dot_body() const { return _state.v_dot_body_v[0]; }
    inline double get_V_dot_body() const { return _state.v_dot_body_v[1]; }
    inline double get_W_dot_body() const { return _state.v_dot_body_v[2]; }

    inline double get_A_X_cg() const { return _state.a_cg_body_v[0]; }
    inline double get_A_Y_cg() const { return _state.a_cg_body_v[1]; }
    inline double get_A_Z_cg() const { return _state.a_cg_body_v[2]; }

    inline double get_A_X_pilot() const { return _state.a_pilot_body_v[0]; }
    inline double get_A_Y_pilot() const { return _state.a_pilot_body_v[1]; }
    inline double get_A_Z_pilot() const { return _state.a_pilot_body_v[2]; }

    inline double get_N_X_cg() const { return _state.n_cg_body_v[0]; }
    inline double get_N_Y_cg() const { return _state.n_cg_body_v[1]; }
    inline double get_N_Z_cg() const { return _state.n_cg_body_v[2]; }

    inline double get_Nlf(void) const { return _state.nlf; }

    // ========== Velocities ==========

    inline double get_V_north() const { return _state.v_local_v[0]; }
    inline double get_V_east() const { return _state.v_local_v[1]; }
    inline double get_V_down() const { return _state.v_local_v[2]; }
    inline double get_uBody () const { return _state.v_body_v[0]; }
    inline double get_vBody () const { return _state.v_body_v[1]; }
    inline double get_wBody () const { return _state.v_body_v[2]; }

    // Please dont comment these out. fdm=ada uses these (see
    // cockpit.cxx) --->
    inline double get_V_north_rel_ground() const {
        return _state.v_local_rel_ground_v[0];
    }
    inline double get_V_east_rel_ground() const {
        return _state.v_local_rel_ground_v[1];
    }
    inline double get_V_down_rel_ground() const {
        return _state.v_local_rel_ground_v[2];
    }
    // <--- fdm=ada uses these (see cockpit.cxx)

    inline double get_V_north_airmass() const { return _state.v_local_airmass_v[0]; }
    inline double get_V_east_airmass() const { return _state.v_local_airmass_v[1]; }
    inline double get_V_down_airmass() const { return _state.v_local_airmass_v[2]; }

    inline double get_U_body() const { return _state.v_body_v[0]; }
    inline double get_V_body() const { return _state.v_body_v[1]; }
    inline double get_W_body() const { return _state.v_body_v[2]; }

    inline double get_V_rel_wind() const { return _state.v_rel_wind; }

    inline double get_V_true_kts() const { return _state.v_true_kts; }

    inline double get_V_ground_speed() const { return _state.v_ground_speed; }
    inline double get_V_ground_speed_kt() const { return _state.v_ground_speed * SG_FEET_TO_METER * 3600 * SG_METER_TO_NM; }
    inline void   set_V_ground_speed_kt(double ground_speed) { _state.v_ground_speed = ground_speed / ( SG_FEET_TO_METER * 3600 * SG_METER_TO_NM); }

    inline double get_V_equiv_kts() const { return _state.v_equiv_kts; }

    inline double get_V_calibrated_kts() const { return _state.v_calibrated_kts; }

    inline double get_P_body() const { return _state.omega_body_v[0]; }
    inline double get_Q_body() const { return _state.omega_body_v[1]; }
    inline double get_R_body() const { return _state.omega_body_v[2]; }

    inline double get_Phi_dot() const { return _state.euler_rates_v[0]; }
    inline double get_Theta_dot() const { return _state.euler_rates_v[1]; }
    inline double get_Psi_dot() const { return _state.euler_rates_v[2]; }
    inline double get_Phi_dot_degps() const { return _state.euler_rates_v[0] * SGD_RADIANS_TO_DEGREES; }
    inline double get_Theta_dot_degps() const { return _state.euler_rates_v[1] * SGD_RADIANS_TO_DEGREES; }
    inline double get_Psi_dot_degps() const { return _state.euler_rates_v[2] * SGD_RADIANS_TO_DEGREES; }

    inline double get_Latitude_dot() const { return _state.geocentric_rates_v[0]; }
    inline double get_Longitude_dot() const { return _state.geocentric_rates_v[1]; }
    inline double get_Radius_dot() const { return _state.geocentric_rates_v[2]; }

    // ========== Positions ==========

    inline double get_Lat_geocentric() const {
        return _state.geocentric_position_v.getLatitudeRad();
    }
    inline double get_Lon_geocentric() const {
        return _state.geocentric_position_v.getLongitudeRad();
    }
    inline double get_Radius_to_vehicle() const {
        return _state.geocentric_position_v.getRadiusFt();
    }

    const SGGeod& getPosition() const { return _state.geodetic_position_v; }
    const SGGeoc& getGeocPosition() const { return _state.geocentric_position_v; }
    const SGVec3d& getCartPosition() const { return _state.cartesian_position_v; }

    inline double get_Latitude() const {
        return _state.geodetic_position_v.getLatitudeRad();
    }
    inline double get_Longitude() const {
        return _state.geodetic_position_v.getLongitudeRad();
    }
    inline double get_Altitude() const {
        return _state.geodetic_position_v.getElevationFt();
    }
    inline double get_Altitude_AGL(void) const { return _state.altitude_agl; }
    inline double get_Track(void) const { return _state.track; }
    inline double get_Path(void) const { return _state.path; }

    inline double get_Latitude_deg () const {
      return _state.geodetic_position_v.getLatitudeDeg();
    }
    inline double get_Longitude_deg () const {
      return _state.geodetic_position_v.getLongitudeDeg();
    }

    inline double get_Phi() const { return _state.euler_angles_v[0]; }
    inline double get_Theta() const { return _state.euler_angles_v[1]; }
    inline double get_Psi() const { return _state.euler_angles_v[2]; }
    inline double get_Phi_deg () const { return get_Phi() * SGD_RADIANS_TO_DEGREES; }
    inline double get_Theta_deg () const { return get_Theta() * SGD_RADIANS_TO_DEGREES; }
    inline double get_Psi_deg () const { return get_Psi() * SGD_RADIANS_TO_DEGREES; }


    // ========== Miscellaneous quantities ==========

    inline double get_Alpha() const { return _state.alpha; }
    inline double get_Alpha_deg() const { return _state.alpha * SGD_RADIANS_TO_DEGREES; }
    inline double get_Beta() const { return _state.beta; }
    inline double get_Beta_deg() const { return _state.beta * SGD_RADIANS_TO_DEGREES; }
    inline double get_Gamma_vert_rad() const { return _state.gamma_vert_rad; }

    inline double get_Density() const { return _state.density; }
    inline double get_Mach_number() const { return _state.mach_number; }

    inline double get_Static_pressure() const { return _state.static_pressure; }
    inline double get_Total_pressure() const { return _state.total_pressure; }
    inline double get_Dynamic_pressure() const { return _state.dynamic_pressure; }

    inline double get_Static_temperature() const { return _state.static_temperature; }
    inline double get_Total_temperature() const { return _state.total_temperature; }

    inline double get_Sea_level_radius() const { return _state.sea_level_radius; }
    inline double get_Earth_position_angle() const {
        return _state.earth_position_angle;
    }

    inline double get_Runway_altitude() const { return _state.runway_altitude; }
    inline double get_Runway_altitude_m() const { return SG_FEET_TO_METER * _state.runway_altitude; }

    inline double get_Climb_Rate() const { return _state.climb_rate; }

    // Note that currently this is the "same" value runway altitude...
    inline double get_ground_elev_ft() const { return _state.runway_altitude; }


    //////////////////////////////////////////////////////////////////////////
    // Ground handling routines
    //////////////////////////////////////////////////////////////////////////

    // Prepare the ground cache for the wgs84 position pt_*.
    // That is take all vertices in the ball with radius rad around the
    // position given by the pt_* and store them in a local scene graph.
    bool prepare_ground_cache_m(double startSimTime, double endSimTime,
                                const double pt[3], double rad);
    bool prepare_ground_cache_ft(double startSimTime, double endSimTime,
                                 const double pt[3], double rad);


    // Returns true if the cache is valid.
    // Also the reference time, point and radius values where the cache
    // is valid for are returned.
    bool is_valid_m(double *ref_time, double pt[3], double *rad);
    bool is_valid_ft(double *ref_time, double pt[3], double *rad);

    // Return the nearest catapult to the given point
    // pt in wgs84 coordinates.
    double get_cat_m(double t, const double pt[3],
                     double end[2][3], double vel[2][3]);
    double get_cat_ft(double t, const double pt[3],
                      double end[2][3], double vel[2][3]);
  

    // Return the orientation and position matrix and the linear and angular
    // velocity of that local coordinate systems origin for a given time and
    // body id. The velocities are in the wgs84 frame at the bodys origin.
    bool get_body_m(double t, simgear::BVHNode::Id id, double bodyToWorld[16],
                    double linearVel[3], double angularVel[3]);


    // Return the altitude above ground below the wgs84 point pt
    // Search for the nearest triangle to pt in downward direction.
    // Return ground properties. The velocities are in the wgs84 frame at the
    // contact point.
    bool get_agl_m(double t, const double pt[3], double max_altoff,
                   double contact[3], double normal[3], double linearVel[3],
                   double angularVel[3], simgear::BVHMaterial const*& material,
                   simgear::BVHNode::Id& id);
    bool get_agl_ft(double t, const double pt[3], double max_altoff,
                    double contact[3], double normal[3], double linearVel[3],
                    double angularVel[3], simgear::BVHMaterial const*& material,
                    simgear::BVHNode::Id& id);
    double get_groundlevel_m(double lat, double lon, double alt);
    double get_groundlevel_m(const SGGeod& geod);


    // Return the nearest point in any direction to the point pt with a maximum
    // distance maxDist. The velocities are in the wgs84 frame at the query
    // position pt.
    bool get_nearest_m(double t, const double pt[3], double maxDist,
                       double contact[3], double normal[3], double linearVel[3],
                       double angularVel[3], simgear::BVHMaterial const*& material,
                       simgear::BVHNode::Id& id);
    bool get_nearest_ft(double t, const double pt[3], double maxDist,
                        double contact[3], double normal[3],double linearVel[3],
                        double angularVel[3], simgear::BVHMaterial const*& material,
                        simgear::BVHNode::Id& id);


    // Return 1 if the hook intersects with a wire.
    // That test is done by checking if the quad spanned by the points pt*
    // intersects with the line representing the wire.
    // If the wire is caught, the cache will trace this wires endpoints until
    // the FDM calls release_wire().
    bool caught_wire_m(double t, const double pt[4][3]);
    bool caught_wire_ft(double t, const double pt[4][3]);
  
    // Return the location and speed of the wire endpoints.
    bool get_wire_ends_m(double t, double end[2][3], double vel[2][3]);
    bool get_wire_ends_ft(double t, double end[2][3], double vel[2][3]);

    // Tell the cache code that it does no longer need to care for
    // the wire end position.
    void release_wire(void);

    // Ground reactions
    bool updateGroundReactions(simgear::BVHMaterial const*& material) {
        return _groundReactions.update(material);
    }
    inline void setHeading(float h) { _groundReactions.setHeading(h); }
    inline void setPosition(double pt[3]) {
        _groundReactions.setPosition(pt);
    }
    inline float getPressure() { return _groundReactions.getPressure(); }
    inline float getBumpiness() { return _groundReactions.getBumpiness(); }
    inline float getGroundDisplacement() {
        return _groundReactions.getGroundDisplacement();
    }
    inline float getStaticFrictionFactor() {
        return _groundReactions.getStaticFrictionFactor();
    }
    inline float getRolingFrictionFactor() {
        return _groundReactions.getRolingFrictionFactor();
    }
    inline bool getSolid() { return _groundReactions.getSolid(); }


    // Manages the AI wake computations.
    void add_ai_wake(FGAIAircraft* ai) { wake_group.AddAI(ai); }
    void reset_wake_group(void) { wake_group.gc(); }
    const AIWakeGroup& get_wake_group(void) { return wake_group; }
};

#endif // _FLIGHT_HXX
