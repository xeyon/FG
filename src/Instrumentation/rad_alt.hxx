/*
 * SPDX-License-Identifier: GPL-2.0+
 * SPDX-FileCopyrightText: 2008 (C) Vivain MEAZZA - vivian.meazza@lineone.net
 * 
 * Radar Altimeter
 *  Written by Vivian MEAZZA, started Feb 2008.
 * 
 * Copyright (C) 2008  Vivain MEAZZA - vivian.meazza@lineone.net
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
*/

#pragma once

#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include <simgear/math/SGMath.hxx>

class RadarAltimeter : public SGSubsystem
{
public:
    RadarAltimeter ( SGPropertyNode *node );
    virtual ~RadarAltimeter ();

    // Subsystem API.
    void init() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "radar-altimeter"; }

private:
    void update_altitude();
    void updateSetHeight();

    double getDistanceAntennaToHit(const SGVec3d& h) const;
    SGVec3d getCartAntennaPos()const;

    SGVec3d rayVector(double az, double el) const;

    SGPropertyNode_ptr _Instrument;
    SGPropertyNode_ptr _user_alt_agl_node;
    SGPropertyNode_ptr _rad_alt_warning_node;
    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _sceneryLoaded;

    SGVec3d _antennaOffset; // in aircraft local XYZ frame

    std::string _name;
    int _num;
    double _time;
    double _interval;

    double _min_radalt;
};
