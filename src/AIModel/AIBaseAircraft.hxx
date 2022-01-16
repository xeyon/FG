// FGAIBaseAircraft - abstract base class for AI aircraft
// Written by Stuart Buchanan, started August 2002
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

#include "AIBase.hxx"


class FGAIBaseAircraft : public FGAIBase
{

public:
    FGAIBaseAircraft(object_type otype = object_type::otAircraft);

    void bind() override;

    // Note that this is mapped to all 6 gear indices gear/gear[0..5]
    void GearPos(double pos)      { m_gearPos = pos; };
    void FlapsPos(double pos)      { m_flapsPos = pos; };
    void SpoilerPos(double pos)    { m_spoilerPos = pos; };
    void SpeedBrakePos(double pos) { m_speedbrakePos = pos; };
    void BeaconLight(bool light)   { m_beaconLight = light; };
    void LandingLight(bool light)  { m_landingLight = light; };
    void NavLight(bool light)      { m_navLight = light; };
    void StrobeLight(bool light)   { m_strobeLight = light; };
    void TaxiLight(bool light)     { m_taxiLight = light; };
    void CabinLight(bool light)    { m_cabinLight = light; };

    double GearPos()       const { return m_gearPos; };
    double FlapsPos()      const { return m_flapsPos; };
    double SpoilerPos()    const { return m_spoilerPos; };
    double SpeedBrakePos() const { return m_speedbrakePos; };
    bool   BeaconLight()   const { return m_beaconLight; };
    bool   LandingLight()  const { return m_landingLight; };
    bool   NavLight()      const { return m_navLight; };
    bool   StrobeLight()   const { return m_strobeLight; };
    bool   TaxiLight()     const { return m_taxiLight; };
    bool   CabinLight()    const { return m_cabinLight; };

protected:

    // Aircraft properties.  
    double m_gearPos;
    double m_flapsPos;
    double m_spoilerPos;
    double m_speedbrakePos;

    // Light properties.
    bool m_beaconLight;
    bool m_cabinLight;
    bool m_landingLight;
    bool m_navLight;
    bool m_strobeLight;
    bool m_taxiLight;
};

