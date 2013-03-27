// FGMouseInput.hxx -- handle user input from mouse devices
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// Copyright (C) 2009 Torsten Dreyer, Torsten (at) t3r _dot_ de
// Copyright (C) 2001 David Megginson, david@megginson.com
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


#ifndef _FGMOUSEINPUT_HXX
#define _FGMOUSEINPUT_HXX

#include "FGCommonInput.hxx"

#include <memory>

#include <simgear/structure/subsystem_mgr.hxx>

// forward decls
namespace osgGA { class GUIEventAdapter; }

////////////////////////////////////////////////////////////////////////
// The Mouse Input Class
////////////////////////////////////////////////////////////////////////
class FGMouseInput : public SGSubsystem, FGCommonInput {
public:
  FGMouseInput();
  virtual ~FGMouseInput();

  virtual void init();
  virtual void update( double dt );

    void doMouseClick (int b, int updown, int x, int y, bool mainWindow, const osgGA::GUIEventAdapter* ea);
    void doMouseMotion (int x, int y, const osgGA::GUIEventAdapter*);
private:
  void processMotion(int x, int y, const osgGA::GUIEventAdapter* ea);
    
  class FGMouseInputPrivate;
  std::auto_ptr<FGMouseInputPrivate> d;
  
};

#endif
