// FGAIGroundVehicle - FGAIShip-derived class creates an AI Ground Vehicle
// by adding a ground following utility
//
// Written by Vivian Meazza, started August 2009.
// - vivian.meazza at lineone.net
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

#pragma once

#include <list>
#include <string>

#include <simgear/compiler.h>

#include "AIBase.hxx"

#include "AIShip.hxx"

#include "AIBase.hxx"
#include "AIManager.hxx"

class FGAIEscort : public FGAIShip
{
public:
    FGAIEscort();
    virtual ~FGAIEscort() = default;

    const char* getTypeString(void) const override { return "escort"; }
    void readFromScenario(SGPropertyNode* scFileNode) override;

    bool init(ModelSearchOrder searchOrder) override;
    void bind() override;
    void reinit() override;
    void update(double dt) override;

private:
    void setStnRange(double r);
    void setStnBrg(double y);
    void setStationSpeed();
    void setStnLimit(double l);
    void setStnAngleLimit(double l);
    void setStnSpeed(double s);
    void setStnHtFt(double h);
    void setStnPatrol(bool p);
    void setStnDegTrue(bool t);
    void setParent();

    void setMaxSpeed(double m);
    void setUpdateInterval(double i);

    void RunEscort(double dt);

    bool getGroundElev(SGGeod inpos);

    SGVec3d getCartHitchPosAt(const SGVec3d& off) const;

    void calcRangeBearing(double lat, double lon, double lat2, double lon2,
        double &range, double &bearing) const;
    double calcTrueBearingDeg(double bearing, double heading);

    SGGeod _selectedpos;
    SGGeod _tgtpos;

    bool _solid = true; // if true ground is solid for FDMs
    double _tgtrange = 0.0;
    double _tgtbrg = 0.0;
    double _ht_agl_ft = 0.0;
    double _relbrg = 0.0;
    double _parent_speed = 0.0;
    double _parent_hdg = 0.0;
    double _interval = 0.0;

    double _stn_relbrg = 0.0;
    double _stn_truebrg = 0.0;
    double _stn_brg = 0.0;
    double _stn_range = 0.0;
    double _stn_height = 0.0;
    double _stn_speed = 0.0;
    double _stn_angle_limit = 0.0;
    double _stn_limit = 0.0;

    bool _MPControl = false;
    bool _patrol = false;
    bool _stn_deg_true = false;
};

