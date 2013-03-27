// autopilot.hxx - an even more flexible, generic way to build autopilots
//
// Written by Torsten Dreyer
// Based heavily on work created by Curtis Olson, started January 2004.
//
// Copyright (C) 2004  Curtis L. Olson  - http://www.flightgear.org/~curt
// Copyright (C) 2010  Torsten Dreyer - Torsten (at) t3r (dot) de
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
#ifndef __AUTOPILOT_HXX
#define __AUTOPILOT_HXX 1

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

namespace FGXMLAutopilot {

class Component;
  
/**
 * @brief A SGSubsystemGroup implementation to serve as a collection
 * of Components
 */
class Autopilot : public SGSubsystemGroup
{
public:
    Autopilot( SGPropertyNode_ptr rootNode, SGPropertyNode_ptr configNode = NULL );
    ~Autopilot();

    void bind();
    void unbind();
    void update( double dt );

    void set_serviceable( bool value ) { _serviceable = value; }
    bool is_serviceable() const { return _serviceable; }

    std::string get_name() const { return _name; }
    void set_name( const std::string & name ) { _name = name; }

    void add_component( Component * component, double updateInterval );

protected:

private:
    std::string _name;
    bool _serviceable;
    SGPropertyNode_ptr _rootNode;
};

}

#endif // __AUTOPILOT_HXX 1
