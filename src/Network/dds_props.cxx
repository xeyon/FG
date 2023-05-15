// dds_props.cxx -- FGFS "DDS" properties protocal class
//
// Written by Erik Hofman, started April 2021
//
// Copyright (C) 2021 by Erik Hofman <erik@ehofman.com>
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
//
// $Id$


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/structure/exception.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/io/iochannel.hxx>
#include <simgear/props/props.hxx>

#include <simgear/io/SGDataDistributionService.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

#include "dds_props.hxx"

// open hailing frequencies
bool FGDDSProps::open() {
    if (is_enabled()) {
        SG_LOG(SG_IO, SG_ALERT, "This shouldn't happen, but the channel "
                << "is already in use, ignoring");
        return false;
    }

    SGIOChannel *io = get_io_channel();

    SG_DDS_Topic *dds = static_cast<SG_DDS_Topic*>(io);
    dds->setup(prop, &FG_DDS_prop_desc);

    // always send and recieve.
    if (!io->open(SG_IO_BI)) {
        SG_LOG(SG_IO, SG_ALERT, "Error opening channel communication layer.");
        return false;
    }

    set_enabled(true);

    return true;
}

// process work for this port
bool FGDDSProps::process() {
    SGIOChannel *io = get_io_channel();

    int length = sizeof(prop);
    char *buf = reinterpret_cast<char*>(&prop);

    if (get_direction() == SG_IO_IN)
    {
        // act as a client: send a request and wait for an answer.

    }
    else if (get_direction() == SG_IO_OUT || get_direction() == SG_IO_BI)
    {
        // act as a server: read requests and send the results.
        while (io->read(buf, length) &&
                prop.version == FG_DDS_PROP_VERSION &&
                prop.mode == FG_DDS_MODE_READ)
        {
            // s is used to keep a copy of the string returned by
            // p->getStringValue() in setProp until it is sent to the DDS layer.
            std::string s;

            if (prop.id == FG_DDS_PROP_REQUEST)
            {
                if (prop.val._d == FG_DDS_STRING)
                {
                    const char *path = prop.val._u.String;
                    auto it = path_list.find(path);
                    if (it == path_list.end())
                    {
                        SGPropertyNode_ptr props = globals->get_props();
                        SGPropertyNode_ptr p = props->getNode(path);
                        if (p)
                        {
                            prop.id = prop_list.size();
                            try {
                                prop_list.push_back(p);
                                path_list[prop.val._u.String] = prop.id;

                            } catch (sg_exception&) {
                                SG_LOG(SG_IO, SG_ALERT, "out of memory");
                            }
                        }
                        setProp(prop, p, s);
                    }
                    else
                    {
                        prop.id = std::distance(path_list.begin(), it);
                        setProp(prop, prop_list[prop.id], s);
                    }
                }
                else
                {
                    SG_LOG(SG_IO, SG_DEBUG, "Recieved a mangled DDS sample.");
                    setProp(prop, nullptr, s);
                }
            }
            else {
                setProp(prop, prop_list[prop.id], s);
            }

            // send the response.
            if (!io->write(buf, length)) {
                SG_LOG(SG_IO, SG_ALERT, "Error writing data.");
            }
        } // while
    }

    return true;
}

// close the channel
bool FGDDSProps::close() {
    SGIOChannel *io = get_io_channel();

    set_enabled(false);

    if (! io->close()) {
        return false;
    }

    return true;
}

void FGDDSProps::setProp(FG_DDS_prop& prop, SGPropertyNode_ptr p, std::string& s)
{
//  prop.id = FG_DDS_PROP_REQUEST;
    prop.version = FG_DDS_PROP_VERSION;
    prop.mode = FG_DDS_MODE_WRITE;
    if (p)
    {
        simgear::props::Type type = p->getType();
        if (type == simgear::props::BOOL) {
            prop.val._d = FG_DDS_BOOL;
            prop.val._u.Bool = p->getBoolValue();
        } else if (type == simgear::props::INT) {
            prop.val._d = FG_DDS_INT;
            prop.val._u.Int32 = p->getIntValue();
        } else if (type == simgear::props::LONG) {
            prop.val._d = FG_DDS_LONG;
            prop.val._u.Int64 = p->getLongValue();
        } else if (type == simgear::props::FLOAT) {
            prop.val._d = FG_DDS_FLOAT;
            prop.val._u.Float32 = p->getFloatValue();
        } else if (type == simgear::props::DOUBLE) {
            prop.val._d = FG_DDS_DOUBLE;
            prop.val._u.Float64 = p->getDoubleValue();
        } else if (type == simgear::props::ALIAS) {
            prop.val._d = FG_DDS_ALIAS;
            s = p->getStringValue();
            prop.val._u.String = const_cast<char*>(s.c_str());
        } else if (type == simgear::props::STRING) {
            prop.val._d = FG_DDS_STRING;
            s = p->getStringValue();
            prop.val._u.String = const_cast<char*>(s.c_str());
        } else if (type == simgear::props::UNSPECIFIED) {
            prop.val._d = FG_DDS_UNSPECIFIED;
            s = p->getStringValue();
            prop.val._u.String = const_cast<char*>(s.c_str());
        } else {
            prop.val._d = FG_DDS_NONE;
            prop.val._u.Int32 = 0;
        }
    }
}


