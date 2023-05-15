/*
 * SPDX-License-Identifier: GPL-2.0+
 * SPDX-FileCopyrightText: (C) Curtis L. Olson - http://www.flightgear.org/~curt
 * 
 * commradio.hxx -- class to manage a nav radio instance
 * 
 * Written by Torsten Dreyer, started February 2014
 * 
 * Copyright (C) Curtis L. Olson - http://www.flightgear.org/~curt
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
 * 
 * $Id$
*/

#pragma once

#include <simgear/props/props.hxx>
#include <Instrumentation/AbstractInstrument.hxx>

namespace Instrumentation {

class SignalQualityComputer : public SGReferenced
{
public:
    virtual ~SignalQualityComputer();
    virtual double computeSignalQuality( double distance_nm ) const = 0;
};

typedef SGSharedPtr<SignalQualityComputer> SignalQualityComputerRef;

class CommRadio : public AbstractInstrument
{
public:
    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "comm-radio"; }

    static SGSubsystem * createInstance( SGPropertyNode_ptr rootNode );
};

}

