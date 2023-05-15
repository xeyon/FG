/**************************************************************************
 * gui.cxx
 *
 * Written 1998 by Durk Talsma, started Juni, 1998.  For the flight gear
 * project.
 *
 * Additional mouse supported added by David Megginson, 1999.
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
 **************************************************************************/


#include <config.h>

#include <simgear/compiler.h>

#include <string>

#include <simgear/structure/exception.hxx>
#include <simgear/structure/commands.hxx>

#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>

#include <Main/main.hxx>
#include <Main/globals.hxx>
#include <Main/locale.hxx>
#include <Main/fg_props.hxx>
#include <Viewer/WindowSystemAdapter.hxx>
#include <Viewer/CameraGroup.hxx>
#include <GUI/new_gui.hxx>
#include <GUI/FGFontCache.hxx>
#include <Viewer/PUICamera.hxx>

#include "gui.h"
#include "layout.hxx"

#include <osg/GraphicsContext>
#include <osg/GLExtensions>

using namespace flightgear;

#if defined(HAVE_PUI)

puFont guiFnt = 0;

// this is declared in puLocal.h, re-declare here so we can call it ourselves
void puSetPasteBuffer  ( const char *ch ) ;

#endif

/* -------------------------------------------------------------------------
init the gui
_____________________________________________________________________*/

namespace
{

#if defined(HAVE_PUI)
class PUIInitOperation : public GraphicsContextOperation
{
public:
    PUIInitOperation() : GraphicsContextOperation(std::string("GUI init"))
    {
    }
    void run(osg::GraphicsContext* gc)
    {
        flightgear::PUICamera::initPUI();
        
        puSetDefaultStyle         ( PUSTYLE_SMALL_SHADED ); //PUSTYLE_DEFAULT
        puSetDefaultColourScheme  (0.8, 0.8, 0.9, 1);

        // ensure we set a non-NULL paste buffer so Ctrl-V doesn't crash us,
        // with older versions of PLIB (pre 1.8.6)
        puSetPasteBuffer(" ");

        // OSG Texture2D sets this, which breaks PLIB fntLoadTXF
        // force it back to zero so width passed to glTexImage2D is used
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        FGFontCache *fc = FGFontCache::instance();
        fc->initializeFonts();
        puFont *GuiFont
            = fc->get(globals->get_locale()->getDefaultFont("typewriter.txf"),
                      15);
        puSetDefaultFonts(*GuiFont, *GuiFont);
        guiFnt = puGetDefaultLabelFont();

        LayoutWidget::setDefaultFont(GuiFont, 15);
  
        if (!fgHasNode("/sim/startup/mouse-pointer")) {
            // no preference specified for mouse pointer
        } else if ( !fgGetBool("/sim/startup/mouse-pointer") ) {
            // don't show pointer
        } else {
            // force showing pointer
            puShowCursor();
        }
    }
};

osg::ref_ptr<PUIInitOperation> initOp;

#endif

// Operation for querying OpenGL parameters. This must be done in a
// valid OpenGL context, potentially in another thread.

struct GeneralInitOperation : public GraphicsContextOperation
{
    GeneralInitOperation()
        : GraphicsContextOperation(std::string("General init"))
    {
    }
    void run(osg::GraphicsContext* gc)
    {
        SGPropertyNode* simRendering = fgGetNode("/sim/rendering");

        simRendering->setStringValue("gl-vendor", (char*) glGetString(GL_VENDOR));
        SG_LOG( SG_GENERAL, SG_INFO, glGetString(GL_VENDOR));

        simRendering->setStringValue("gl-renderer", (char*) glGetString(GL_RENDERER));
        SG_LOG( SG_GENERAL, SG_INFO, glGetString(GL_RENDERER));

        simRendering->setStringValue("gl-version", (char*) glGetString(GL_VERSION));
        SG_LOG( SG_GENERAL, SG_INFO, glGetString(GL_VERSION));

        // Old hardware without support for OpenGL 2.0 does not support GLSL and
        // glGetString returns NULL for GL_SHADING_LANGUAGE_VERSION.
        //
        // See http://flightgear.org/forums/viewtopic.php?f=17&t=19670&start=15#p181945
        const char* glsl_version = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
        if( !glsl_version )
          glsl_version = "UNSUPPORTED";
        simRendering->setStringValue("gl-shading-language-version", glsl_version);
        SG_LOG( SG_GENERAL, SG_INFO, glsl_version);

        GLint tmp;
        glGetIntegerv( GL_MAX_TEXTURE_SIZE, &tmp );
        simRendering->setIntValue("max-texture-size", tmp);

        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &tmp);
        simRendering->setIntValue("max-texture-units", tmp);

        glGetIntegerv( GL_DEPTH_BITS, &tmp );
        simRendering->setIntValue("depth-buffer-bits", tmp);
        
        const osg::GLExtensions* extensions = gc->getState()->get<osg::GLExtensions>();
        if (extensions->glVertexAttribDivisor) {
            SG_LOG( SG_GENERAL, SG_INFO, "VertexAttribDivisor supported");
        }
        
    }
};

}

/** Initializes GUI.
 * Returns true when done, false when still busy (call again). */
bool guiInit(osg::GraphicsContext* gc)
{
    static osg::ref_ptr<GeneralInitOperation> genOp;
    static bool didInit = false;
    
    if (didInit) {
        return true;
    }
    
    if (!genOp.valid())
    {
        // Pick some window on which to do queries.
        // XXX Perhaps all this graphics initialization code should be
        // moved to renderer.cxx?
        genOp = new GeneralInitOperation;
        if (gc) {
            gc->add(genOp.get());
#if defined(HAVE_PUI)
            initOp = new PUIInitOperation;
            gc->add(initOp.get());
#endif
        } else {
            WindowSystemAdapter* wsa = WindowSystemAdapter::getWSA();
            wsa->windows[0]->gc->add(genOp.get());
        }
        return false; // not ready yet
    }
    else
    {
        if (!genOp->isFinished())
            return false;
#if defined(HAVE_PUI)
        if (!initOp.valid())
            return true;
        if (!initOp->isFinished())
            return false;
        initOp = 0;
#endif
        genOp = 0;
        didInit = true;
        // we're done
        return true;
    }
}

void syncPausePopupState()
{
    bool paused = fgGetBool("/sim/freeze/master",true) || fgGetBool("/sim/freeze/clock",true);
    SGPropertyNode_ptr args(new SGPropertyNode);
    args->setStringValue("id", "sim-pause");
    if (paused && fgGetBool("/sim/view-name-popup")) {
      args->setStringValue("label", "Simulation is paused");
      globals->get_commands()->execute("show-message", args, nullptr);
    } else {
      globals->get_commands()->execute("clear-message", args, nullptr);
    }

}
