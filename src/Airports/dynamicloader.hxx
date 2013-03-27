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

#ifndef _DYNAMIC_LOADER_HXX_
#define _DYNAMIC_LOADER_HXX_

#include <simgear/xml/easyxml.hxx>

#include "dynamics.hxx"
#include <Navaids/positioned.hxx>

class FGAirportDynamicsXMLLoader : public XMLVisitor {
public:
    FGAirportDynamicsXMLLoader(FGAirportDynamics* dyn);

protected:
    virtual void startXML (); 
    virtual void endXML   ();
    virtual void startElement (const char * name, const XMLAttributes &atts);
    virtual void endElement (const char * name);
    virtual void data (const char * s, int len);
    virtual void pi (const char * target, const char * data);
    virtual void warning (const char * message, int line, int column);
    virtual void error (const char * message, int line, int column);

private:
    void startParking(const XMLAttributes &atts);    
    void startNode(const XMLAttributes &atts);
    void startArc(const XMLAttributes &atts);
  
    FGAirportDynamics* _dynamics;
    string value;
  
    // map from local (groundnet.xml) to global (nav-cache) IDs for nodes
    std::map<int, PositionedID> _idMap;
  
  // data integrity - watch for unreferenced nodes and duplicated edges
    typedef std::pair<int, int> IntPair;
    std::set<IntPair> _arcSet;
  
    std::set<PositionedID> _unreferencedNodes;
  
    // map from allocated parking position to its local push-back node
    // used to defer binding the push-back node until we've processed
    // all nodes
    std::map<PositionedID, int> _parkingPushbacks;
};

#endif
