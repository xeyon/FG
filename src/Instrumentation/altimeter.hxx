/*
 * SPDX-License-Identifier: CC0-1.0
 * 
 * altimeter.hxx - an altimeter tied to the static port.
 * Written by David Megginson, started 2002.
 * Updated by John Denker to match changes in altimeter.cxx in 2007
 * 
 * This file is in the Public Domain and comes with no warranty.
*/

#pragma once

#include <simgear/props/props.hxx>
#include <simgear/props/tiedpropertylist.hxx>
#include <simgear/structure/subsystem_mgr.hxx>
#include <Environment/atmosphere.hxx>


/**
 * Model a barometric altimeter tied to the static port.
 *
 * Input properties:
 *
 * /instrumentation/<name>/serviceable
 * /instrumentation/<name>/setting-inhg
 * <static_pressure>
 *
 * Output properties:
 *
 * /instrumentation/<name>/indicated-altitude-ft
 */
class Altimeter : public SGSubsystem
{
public:
    Altimeter (SGPropertyNode *node, const std::string& aDefaultName, double quantum = 0);
    virtual ~Altimeter ();

    // Subsystem API.
    void bind() override;
    void init() override;
    void reinit() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "altimeter"; }

    double getSettingInHg() const;
    void setSettingInHg( double value );
    double getSettingHPa() const;
    void setSettingHPa( double value );

private:
    std::string _name;
    int _num;
    SGPropertyNode_ptr _rootNode;
    std::string _static_pressure;
    double _tau;
    double _quantum;
    double _kollsman;
    double _raw_PA;
    double _settingInHg;
    bool _encodeModeC;
    bool _encodeModeS;

    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _pressure_node;
    SGPropertyNode_ptr _press_alt_node;
    SGPropertyNode_ptr _mode_c_node;
    SGPropertyNode_ptr _mode_s_node;
    SGPropertyNode_ptr _transponder_node;
    SGPropertyNode_ptr _altitude_node;

    FGAltimeter _altimeter;

    simgear::TiedPropertyList _tiedProperties;
};
