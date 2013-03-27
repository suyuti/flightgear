// Integrate Canvas into FlightGear
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

#include "FGCanvasSystemAdapter.hxx"

#include <Main/globals.hxx>
#include <Scripting/NasalSys.hxx>
#include <Viewer/renderer.hxx>

#include <osgDB/ReadFile>
#include <stdexcept>

namespace canvas
{
  //----------------------------------------------------------------------------
  simgear::canvas::FontPtr
  FGCanvasSystemAdapter::getFont(const std::string& name) const
  {
    SGPath path = globals->resolve_resource_path("Fonts/" + name);
    if( path.isNull() )
    {
      SG_LOG
      (
        SG_GL,
        SG_ALERT,
        "canvas::Text: No such font: " << name
      );
      return simgear::canvas::FontPtr();
    }

    SG_LOG
    (
      SG_GL,
      SG_INFO,
      "canvas::Text: using font file " << path.str()
    );

    simgear::canvas::FontPtr font = osgText::readFontFile(path.c_str());
    if( !font )
      SG_LOG
      (
        SG_GL,
        SG_ALERT,
        "canvas::Text: Failed to open font file " << path.c_str()
      );

    return font;
  }

  //----------------------------------------------------------------------------
  void FGCanvasSystemAdapter::addCamera(osg::Camera* camera) const
  {
    globals->get_renderer()->addCamera(camera, false);
  }

  //----------------------------------------------------------------------------
  void FGCanvasSystemAdapter::removeCamera(osg::Camera* camera) const
  {
    globals->get_renderer()->removeCamera(camera);
  }

  //----------------------------------------------------------------------------
  osg::Image* FGCanvasSystemAdapter::getImage(const std::string& path) const
  {
    SGPath tpath = globals->resolve_resource_path(path);
    if( tpath.isNull() || !tpath.exists() )
    {
      SG_LOG(SG_GL, SG_ALERT, "canvas::Image: No such image: " << path);
      return 0;
    }

    return osgDB::readImageFile(tpath.c_str());
  }

  /**
   * Get current FGNasalSys instance.
   */
  static FGNasalSys* getNasalSys()
  {
    static FGNasalSys* nasal_sys = 0;
    // TODO if Nasal is able to be removed and/or recreated at runtime we need
    //      to ensure that always the current instance is used
    if( !nasal_sys )
    {
      nasal_sys = dynamic_cast<FGNasalSys*>(globals->get_subsystem("nasal"));
      if( !nasal_sys )
        throw std::runtime_error("FGCanvasSystemAdapter: no NasalSys");
    }

    return nasal_sys;
  }

  //----------------------------------------------------------------------------
  naContext FGCanvasSystemAdapter::getNasalContext() const
  {
    return getNasalSys()->context();
  }

  //----------------------------------------------------------------------------
  int FGCanvasSystemAdapter::gcSave(naRef r)
  {
    return getNasalSys()->gcSave(r);
  }

  //----------------------------------------------------------------------------
  void FGCanvasSystemAdapter::gcRelease(int key)
  {
    getNasalSys()->gcRelease(key);
  }

  //------------------------------------------------------------------------------
  naRef FGCanvasSystemAdapter::callMethod( naRef code,
                                           naRef self,
                                           int argc,
                                           naRef* args,
                                           naRef locals )
  {
    return getNasalSys()->callMethod(code, self, argc, args, locals);
  }

}
