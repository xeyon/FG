/*
 * SPDX-License-Identifier: CC0-1.0
 * 
 * slip_skid_ball.hxx - an slip-skid ball.
 * Written by David Megginson, started 2003.
 * 
 * This file is in the Public Domain and comes with no warranty.
*/

#pragma once

#ifndef __cplusplus
# error This library requires C++
#endif

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>


/**
 * Model a slip-skid ball.
 *
 * Input properties:
 *
 * /instrumentation/"name"/serviceable
 * /accelerations/pilot/y-accel-fps_sec
 * /accelerations/pilot/z-accel-fps_sec
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-slip-skid
 */
class SlipSkidBall : public SGSubsystem
{
public:
    SlipSkidBall ( SGPropertyNode *node );
    virtual ~SlipSkidBall ();

    // Subsystem API.
    void init() override;
    void reinit() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "slip-skid-ball"; }

private:
    std::string _name;
    int _num;

    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _y_accel_node;
    SGPropertyNode_ptr _z_accel_node;
    SGPropertyNode_ptr _out_node;
    SGPropertyNode_ptr _override_node;
};
