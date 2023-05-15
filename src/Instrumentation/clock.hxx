/*
 * SPDX-License-Identifier: CC0-1.0
 * 
 * clock.hxx.
 * Written by Melchior FRANZ, started 2003.
 * 
 * This file is in the Public Domain and comes with no warranty.
 * 
 * $Id$
*/

#pragma once

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>


/**
 * Model a clock.
 *
 * Input properties:
 *
 * /instrumentation/clock/serviceable
 * /instrumentation/clock/offset-sec
 *
 * Output properties:
 *
 * /instrumentation/clock/indicated-sec
 * /instrumentation/clock/indicated-string
 */
class Clock : public SGSubsystem
{
public:
    Clock(SGPropertyNode *node);
    virtual ~Clock();

    // Subsystem API.
    void init() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "clock"; }

private:
    std::string _name;
    unsigned int _num;

    bool _is_serviceable;
    long _gmt_time_sec;
    long _offset_sec;
    long _indicated_sec;
    long _indicated_min;
    long _indicated_hour;
    long _local_hour;
    char _indicated_string[16];
    char _indicated_short_string[16];
    char _local_short_string[16];
    long _standstill_offset;

    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _offset_node;
    SGPropertyNode_ptr _sec_node;
    SGPropertyNode_ptr _hour_node;
    SGPropertyNode_ptr _lhour_node;
    SGPropertyNode_ptr _min_node;
    SGPropertyNode_ptr _string_node;
    SGPropertyNode_ptr _string_node1;
    SGPropertyNode_ptr _string_node2;
};
