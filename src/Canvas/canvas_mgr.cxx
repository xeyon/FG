// Canvas with 2D rendering api
//
// Copyright (C) 2012  Thomas Geymayer <tomgey@gmail.com>
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

#include "canvas_mgr.hxx"

#include <Cockpit/od_gauge.hxx>
#include <Main/fg_props.hxx>
#include <Scripting/NasalModelData.hxx>
#include <Viewer/CameraGroup.hxx>
#include <Model/modelmgr.hxx>

#include <simgear/canvas/Canvas.hxx>
#include <simgear/scene/model/placement.hxx>

namespace sc = simgear::canvas;

//------------------------------------------------------------------------------
static sc::Placements addSceneObjectPlacement( SGPropertyNode* placement,
                                               sc::CanvasPtr canvas )
{
  int module_id = placement->getIntValue("module-id", -1);
  if( module_id < 0 )
    return sc::Placements();

  FGNasalModelData* model_data =
    FGNasalModelData::getByModuleId( static_cast<unsigned int>(module_id) );

  if( !model_data )
    return sc::Placements();

  if( !model_data->getNode() )
    return sc::Placements();

  return FGODGauge::set_texture
  (
    model_data->getNode(),
    placement,
    canvas->getTexture(),
    canvas->getCullCallback(),
    canvas
  );
}


//------------------------------------------------------------------------------
static sc::Placements addDynamicModelPlacement(SGPropertyNode* placement,
                                              sc::CanvasPtr canvas)
{
    const string dyn_model_path = placement->getStringValue("model-path");
    if (dyn_model_path.empty())
        return {};

    auto  modelmgr = globals->get_subsystem<FGModelMgr>();
    if (!modelmgr)
        return {};

    FGModelMgr::Instance* model_instance = modelmgr->findInstanceByNodePath(dyn_model_path);
    if (!model_instance || !model_instance->model || !model_instance->model->getSceneGraph())
        return {};

    return FGODGauge::set_texture(
        model_instance->model->getSceneGraph(),
        placement,
        canvas->getTexture(),
        canvas->getCullCallback(),
        canvas);
}

//------------------------------------------------------------------------------
CanvasMgr::CanvasMgr():
  simgear::canvas::CanvasMgr( fgGetNode("/canvas/by-index", true) ),
  _cb_model_reinit
  (
    this,
    &CanvasMgr::handleModelReinit,
    fgGetNode("/sim/signals/model-reinit", true)
  )
{
    const SGPath path = globals->get_fg_root() / "gui" / "shaders";
    setShaderRoot(path);
}

//----------------------------------------------------------------------------
void CanvasMgr::init()
{
  // add our two placement factories
  sc::Canvas::addPlacementFactory
  (
   "object", [](SGPropertyNode* placement, sc::CanvasPtr canvas) {
      return FGODGauge::set_aircraft_texture(placement,
                                             canvas->getTexture(),
                                             canvas->getCullCallback(),
                                             canvas);
  });
  sc::Canvas::addPlacementFactory("scenery-object", &addSceneObjectPlacement);
  sc::Canvas::addPlacementFactory("dynamic-model", &addDynamicModelPlacement);

  simgear::canvas::CanvasMgr::init();
}

//----------------------------------------------------------------------------
void CanvasMgr::shutdown()
{
  simgear::canvas::CanvasMgr::shutdown();

  sc::Canvas::removePlacementFactory("object");
  sc::Canvas::removePlacementFactory("scenery-object");
  sc::Canvas::removePlacementFactory("dynamic-model");
}

//------------------------------------------------------------------------------
unsigned int
CanvasMgr::getCanvasTexId(const simgear::canvas::CanvasPtr& canvas) const
{
  if( !canvas )
    return 0;

  osg::Texture2D* tex = canvas->getTexture();
  if( !tex )
    return 0;

//  osgViewer::Viewer::Contexts contexts;
//  globals->get_renderer()->getViewer()->getContexts(contexts);
//
//  if( contexts.empty() )
//    return 0;

  osg::Camera* guiCamera = flightgear::getGUICamera(flightgear::CameraGroup::getDefault());
  if (!guiCamera)
    return 0;

  osg::State* state = guiCamera->getGraphicsContext()->getState(); //contexts[0]->getState();
  if( !state )
    return 0;

  osg::Texture::TextureObject* tobj =
    tex->getTextureObject( state->getContextID() );
  if( !tobj )
    return 0;

  return tobj->_id;
}

//----------------------------------------------------------------------------
void CanvasMgr::handleModelReinit(SGPropertyNode*)
{
    for (size_t i = 0; i < _elements.size(); ++i)
    {
        sc::Canvas* element = static_cast<sc::Canvas*>(_elements[i].get());
        if (element)
            element->reloadPlacements("object");
    }
}


// Register the subsystem.
SGSubsystemMgr::Registrant<CanvasMgr> registrantCanvasMgr(
    SGSubsystemMgr::DISPLAY);
