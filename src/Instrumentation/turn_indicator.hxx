/*
 * SPDX-License-Identifier: CC0-1.0
 * 
 * turn_indicator.hxx - an electric-powered turn indicator.
 * Written by David Megginson, started 2003.
 * 
 * This file is in the Public Domain and comes with no warranty.
 * 
*/


#pragma once

#ifndef __cplusplus
# error This library requires C++
#endif

#include <Instrumentation/AbstractInstrument.hxx>

#include "gyro.hxx"


/**
 * Model an electric-powered turn indicator.
 *
 * This class does not model the slip/skid ball; that is properly
 * a separate instrument.
 *
 * Input properties:
 *
 * /instrumentation/"name"/serviceable
 * /instrumentation/"name"/spin
 * /orientation/roll-rate-degps
 * /orientation/yaw-rate-degps
 * /systems/electrical/outputs/turn-coordinator
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-turn-rate
 */
class TurnIndicator : public AbstractInstrument
{
public:
    TurnIndicator ( SGPropertyNode *node );
    virtual ~TurnIndicator ();

    // Subsystem API.
    //void bind() override;
    void init() override;
    void reinit() override;
    //void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "turn-indicator"; }

private:
    Gyro _gyro;
    double _last_rate;

    SGPropertyNode_ptr _roll_rate_node;
    SGPropertyNode_ptr _yaw_rate_node;
    SGPropertyNode_ptr _rate_out_node;
};
