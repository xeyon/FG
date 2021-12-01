// viewmgr.hxx -- class for managing all the views in the flightgear world.
//
// Written by Curtis Olson, started October 2000.
//
// Copyright (C) 2000  Curtis L. Olson  - http://www.flightgear.org/~curt
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


#ifndef _VIEWMGR_HXX
#define _VIEWMGR_HXX

#include <vector>

#include <simgear/compiler.h>
#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/tiedpropertylist.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/screen/video-encoder.hxx>


// forward decls
namespace flightgear
{
    class View;
    typedef SGSharedPtr<flightgear::View> ViewPtr;
}

// Define a structure containing view information
class FGViewMgr : public SGSubsystem
{
public:
    // Constructor
    FGViewMgr( void );

    // Destructor
    ~FGViewMgr( void );

    // Subsystem API.
    void bind() override;
    void init() override;
    void postinit() override;
    void reinit() override;
    void shutdown() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "view-manager"; }

    // getters
    inline int size() const { return views.size(); }

    int getCurrentViewIndex() const;

    flightgear::View* get_current_view();
    const flightgear::View* get_current_view() const;

    flightgear::View* get_view( int i );
    const flightgear::View* get_view( int i ) const;

    flightgear::View* next_view();
    flightgear::View* prev_view();

    // Support for extra view windows. This only works if --compositer-viewer=1 was specified.
    //
    
    // Calls SviewPush().
    void view_push();
    
    // These all end up calling SviewCreate().
    void clone_current_view(const SGPropertyNode* config);
    void clone_last_pair(const SGPropertyNode* config);
    void clone_last_pair_double(const SGPropertyNode* config);
    void view_new(const SGPropertyNode* config);

    // setters
    void clear();

    void add_view( flightgear::View * v );

    // Start video encoding to <path>.
    //
    // If <name> is "" we generate a name containing current date and time.
    //
    // If <codec> is "" we use /sim/video/codec.
    //
    // If quality is -1 we use /sim/video/quality; similarly for speed. If
    // bitrate is 0 we use /sim/video/bitrate.
    //
    // We show popup warning if values are out of range - quality and speed
    // must be -1 or 0-1, bitrate must be >= 0.
    //
    void video_start(
            const std::string& name,
            const std::string& codec,
            double quality,
            double speed,
            int bitrate
            );
    void video_stop();

private:
    simgear::TiedPropertyList _tiedProperties;

    void setCurrentViewIndex(int newview);

    bool _inited = false;
    std::vector<SGPropertyNode_ptr> config_list;
    SGPropertyNode_ptr _viewNumberProp;
    typedef std::vector<flightgear::ViewPtr> viewer_list;
    viewer_list views;

    int _current = 0;

    std::unique_ptr<simgear::VideoEncoder>  _video_encoder;
};

#endif // _VIEWMGR_HXX
