// Window for placing a Canvas onto it (for dialogs, menus, etc.)
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

#ifndef CANVAS_WINDOW_HXX_
#define CANVAS_WINDOW_HXX_

#include <simgear/canvas/elements/CanvasImage.hxx>
#include <simgear/canvas/MouseEvent.hxx>
#include <simgear/props/PropertyBasedElement.hxx>
#include <simgear/props/propertyObject.hxx>

#include <osg/Geode>
#include <osg/Geometry>

namespace canvas
{
  class Window:
    public simgear::PropertyBasedElement
  {
    public:

      enum Resize
      {
        NONE    = 0,
        LEFT    = 1,
        RIGHT   = LEFT << 1,
        TOP     = RIGHT << 1,
        BOTTOM  = TOP << 1,
        INIT    = BOTTOM << 1
      };

      Window(SGPropertyNode* node);
      virtual ~Window();

      virtual void update(double delta_time_sec);
      virtual void valueChanged(SGPropertyNode* node);

      osg::Group* getGroup();
      const SGRect<float>& getRegion() const;

      void setCanvas(simgear::canvas::CanvasPtr canvas);
      simgear::canvas::CanvasWeakPtr getCanvas() const;

      bool isVisible() const;
      bool isResizable() const;
      bool isCapturingEvents() const;

      bool handleMouseEvent(const simgear::canvas::MouseEventPtr& event);

      void handleResize(uint8_t mode, const osg::Vec2f& delta = osg::Vec2f());

      void doRaise(SGPropertyNode* node_raise = 0);

    protected:

      simgear::canvas::Image _image;
      bool _resizable,
           _capture_events;

      simgear::PropertyObject<int> _resize_top,
                                   _resize_right,
                                   _resize_bottom,
                                   _resize_left,
                                   _resize_status;

  };
} // namespace canvas

#endif /* CANVAS_WINDOW_HXX_ */
