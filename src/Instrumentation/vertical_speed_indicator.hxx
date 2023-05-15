/*
 * SPDX-License-Identifier: CC0-1.0
 * 
 * vertical_speed_indicator.hxx - a regular VSI tied to the static port.
 * Written by David Megginson, started 2002.
 * 
 * Last change by E. van den Berg, 17.02.1013
 * 
 * This file is in the Public Domain and comes with no warranty.
 * 
*/


#pragma once

#ifndef __cplusplus
# error This library requires C++
#endif

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>


/**
 * Model a non-instantaneous VSI tied to the static port.
 *
 * Input properties:
 *
 * /instrumentation/"name"/serviceable
 * "static_port"/pressure-inhg
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-speed-fpm
 * /instrumentation/"name"/indicated-speed-mps
 * /instrumentation/"name"/indicated-speed-kts
 */
class VerticalSpeedIndicator : public SGSubsystem
{
public:
    VerticalSpeedIndicator ( SGPropertyNode *node );
    virtual ~VerticalSpeedIndicator ();

    // Subsystem API.
    void init() override;
    void reinit() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "vertical-speed-indicator"; }

private:
    double _casing_pressure_Pa = 0.0;
    double _casing_airmass_kg = 0.0;
    double _casing_density_kgpm3 = 0.0;
    double _orifice_massflow_kgps = 0.0;

    const std::string _name;
    const int _num;
    std::string _static_pressure;
    std::string _static_temperature;

    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _pressure_node;
    SGPropertyNode_ptr _temperature_node;
    SGPropertyNode_ptr _speed_fpm_node;
    SGPropertyNode_ptr _speed_mps_node;
    SGPropertyNode_ptr _speed_kts_node;
};
