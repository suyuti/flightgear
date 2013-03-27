// autopilot.cxx - an even more flexible, generic way to build autopilots
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "autopilot.hxx"

#include <simgear/structure/StateMachine.hxx>
#include <simgear/sg_inlines.h>

#include "component.hxx"
#include "functor.hxx"
#include "predictor.hxx"
#include "digitalfilter.hxx"
#include "pisimplecontroller.hxx"
#include "pidcontroller.hxx"
#include "logic.hxx"
#include "flipflop.hxx"

#include "Main/fg_props.hxx"

using std::map;
using std::string;

using namespace FGXMLAutopilot;

class StateMachineComponent : public Component
{
public:
  StateMachineComponent(SGPropertyNode_ptr config)
  {
    inner = simgear::StateMachine::createFromPlist(config, globals->get_props());
  }
  
  virtual bool configure( const std::string & nodeName, SGPropertyNode_ptr config)
  {
    return false;
  }
  
  virtual void update( bool firstTime, double dt )
  {
    SG_UNUSED(firstTime);
    inner->update(dt);
  }
  
private:
  simgear::StateMachine_ptr inner;
};

class StateMachineFunctor : public FunctorBase<Component>
{
public:
  virtual ~StateMachineFunctor() {}
  virtual Component* operator()( SGPropertyNode_ptr configNode )
  {
    return new StateMachineComponent(configNode);
  }
};


class ComponentForge : public map<string,FunctorBase<Component> *> {
public:
    virtual ~ ComponentForge();
};

ComponentForge::~ComponentForge()
{
    for( iterator it = begin(); it != end(); ++it )
        delete it->second;
}

static ComponentForge componentForge;

Autopilot::Autopilot( SGPropertyNode_ptr rootNode, SGPropertyNode_ptr configNode ) :
  _name("unnamed autopilot"),
  _serviceable(true),
  _rootNode(rootNode)
{
  if (componentForge.empty())
  {
      componentForge["pid-controller"]       = new CreateAndConfigureFunctor<PIDController,Component>();
      componentForge["pi-simple-controller"] = new CreateAndConfigureFunctor<PISimpleController,Component>();
      componentForge["predict-simple"]       = new CreateAndConfigureFunctor<Predictor,Component>();
      componentForge["filter"]               = new CreateAndConfigureFunctor<DigitalFilter,Component>();
      componentForge["logic"]                = new CreateAndConfigureFunctor<Logic,Component>();
      componentForge["flipflop"]             = new CreateAndConfigureFunctor<FlipFlop,Component>();
      componentForge["state-machine"]        = new StateMachineFunctor();
  }

  if( configNode == NULL ) configNode = rootNode;

  int count = configNode->nChildren();
  for ( int i = 0; i < count; ++i ) {
    SGPropertyNode_ptr node = configNode->getChild(i);
    string childName = node->getName();
    if( componentForge.count(childName) == 0 ) {
      SG_LOG( SG_AUTOPILOT, SG_BULK, "unhandled element <" << childName << ">" << std::endl );
      continue;
    }

    Component * component = (*componentForge[childName])(node);
    if( component->get_name().length() == 0 ) {
      std::ostringstream buf;
      buf <<  "unnamed_component_" << i;
      component->set_name( buf.str() );
    }

    double updateInterval = node->getDoubleValue( "update-interval-secs", 0.0 );

    SG_LOG( SG_AUTOPILOT, SG_DEBUG, "adding  autopilot component \"" << childName << "\" as \"" << component->get_name() << "\" with interval=" << updateInterval );
    add_component(component,updateInterval);
  }
}

Autopilot::~Autopilot() 
{
}

void Autopilot::bind() 
{
  fgTie( _rootNode->getNode("serviceable", true)->getPath().c_str(), this, 
    &Autopilot::is_serviceable, &Autopilot::set_serviceable );
}

void Autopilot::unbind() 
{
  _rootNode->untie( "serviceable" );
}

void Autopilot::add_component( Component * component, double updateInterval )
{
  if( component == NULL ) return;

  // check for duplicate name
  std::string name = component->get_name();
  for( unsigned i = 0; get_subsystem( name.c_str() ) != NULL; i++ ) {
      std::ostringstream buf;
      buf <<  component->get_name() << "_" << i;
      name = buf.str();
  }
  if( name != component->get_name() )
    SG_LOG( SG_ALL, SG_WARN, "Duplicate autopilot component " << component->get_name() << ", renamed to " << name );

  set_subsystem( name.c_str(), component, updateInterval );
}

void Autopilot::update( double dt ) 
{
  if( !_serviceable || dt <= SGLimitsd::min() )
    return;
  SGSubsystemGroup::update( dt );
}
