// CocoaMouseCursor.hxx - mouse cursor using Cocoa APIs

// Copyright (C) 2013 James Turner <zakalawe@mac.com>
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

#ifndef FG_GUI_COCOA_MOUSE_CURSOR_HXX
#define FG_GUI_COCOA_MOUSE_CURSOR_HXX

#include <memory> // for auto_ptr

#include "MouseCursor.hxx"

class CocoaMouseCursor : public FGMouseCursor
{
public:
    CocoaMouseCursor();
    virtual ~CocoaMouseCursor();
    
    virtual void setCursor(Cursor aCursor);
    
    virtual void setCursorVisible(bool aVis);
    
    virtual void hideCursorUntilMouseMove();
    
    virtual void mouseMoved();

private:
    class CocoaMouseCursorPrivate;
    std::auto_ptr<CocoaMouseCursorPrivate> d;
};


#endif
