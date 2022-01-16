// FGAIThermal - FGAIBase-derived class creates an AI thermal
//
// Original by Written by David Culp
//
// An attempt to refine the thermal shape and behaviour by WooT 2009
//
// Copyright (C) 2009 Patrice Poly ( WooT )
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

#include <string_view>

#include "AIBase.hxx"
#include "AIManager.hxx"

#include <string>


class FGAIThermal : public FGAIBase {

public:
	FGAIThermal();
	virtual ~FGAIThermal() = default;

	string_view getTypeString(void) const override { return "thermal"; }
	void readFromScenario(SGPropertyNode* scFileNode) override;

        bool init(ModelSearchOrder searchOrder) override;
	void bind() override;
	void update(double dt) override;

	inline void setMaxStrength( double s ) { max_strength = s; };
	inline void setDiameter( double d ) { diameter = d; };
	inline void setHeight( double h ) { height = h; };
	inline void setMaxUpdraft( double lift ) { v_up_max = lift; };
	inline void setMinUpdraft( double sink ) { v_up_min = sink; };
	inline void setR_up_frac( double r ) { r_up_frac = r; };

	inline double getStrength() const { return strength; };
	inline double getDiameter() const { return diameter; };
	inline double getHeight() const { return height; };
	inline double getV_up_max() const { return v_up_max; };
	inline double getV_up_min() const { return v_up_min; };
	inline double getR_up_frac() const { return r_up_frac; };

	void getGroundElev(double dt);

private:
	void Run(double dt);

	double get_strength_fac(double alt_frac);
	double max_strength;
	double strength;
	double diameter;
	double height = 0.0;
	double factor;
	double alt_rel = 0.0;
	double alt;
	double v_up_max = 0.0;
	double v_up_min = 0.0;
	double r_up_frac = 0.0;
	double cycle_timer;
	double dt_count;
	double time = 0.0;
	double xx = 0.0;
	double ground_elev_ft; // ground level in ft

	bool do_agl_calc = false;
	bool is_forming = false;
	bool is_formed = false;
	bool is_dying = false;
	bool is_dead = false;

	SGPropertyNode_ptr _surface_wind_from_deg_node;
	SGPropertyNode_ptr _surface_wind_speed_node;
	SGPropertyNode_ptr _aloft_wind_from_deg_node;
	SGPropertyNode_ptr _aloft_wind_speed_node;
};
