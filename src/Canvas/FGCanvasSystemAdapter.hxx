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

#ifndef FG_CANVASSYSTEMADAPTER_HXX_
#define FG_CANVASSYSTEMADAPTER_HXX_

#include <simgear/canvas/CanvasSystemAdapter.hxx>

namespace canvas
{
  class FGCanvasSystemAdapter:
    public simgear::canvas::SystemAdapter
  {
    public:
      virtual simgear::canvas::FontPtr getFont(const std::string& name) const;
      virtual void addCamera(osg::Camera* camera) const;
      virtual void removeCamera(osg::Camera* camera) const;
      virtual osg::Image* getImage(const std::string& path) const;

      virtual naContext getNasalContext() const;
      virtual int gcSave(naRef r);
      virtual void gcRelease(int key);
      virtual naRef callMethod( naRef code,
                                naRef self,
                                int argc,
                                naRef* args,
                                naRef locals );
  };
}

#endif /* FG_CANVASSYSTEMADAPTER_HXX_ */
