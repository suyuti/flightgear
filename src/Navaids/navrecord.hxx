// navrecord.hxx -- generic vor/dme/ndb class
//
// Written by Curtis Olson, started May 2004.
//
// Copyright (C) 2004  Curtis L. Olson - http://www.flightgear.org/~curt
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


#ifndef _FG_NAVRECORD_HXX
#define _FG_NAVRECORD_HXX

#include <iosfwd>

#include "navaids_fwd.hxx"
#include "positioned.hxx"
#include <Airports/airports_fwd.hxx>

const double FG_NAV_DEFAULT_RANGE = 50; // nm
const double FG_LOC_DEFAULT_RANGE = 18; // nm
const double FG_DME_DEFAULT_RANGE = 50; // nm
const double FG_NAV_MAX_RANGE = 300;    // nm

class FGNavRecord : public FGPositioned 
{

    int freq;
    int range;
    double multiuse;            // can be slaved variation of VOR
                                // (degrees) or localizer heading
                                // (degrees) or dme bias (nm)

    std::string mName;                // verbose name in nav database
    PositionedID mRunway;        // associated runway, if there is one
    PositionedID mColocated;     // Colocated DME at a navaid (ILS, VOR, TACAN, NDB)
    bool serviceable;		// for failure modeling

  void processSceneryILS(SGPropertyNode* aILSNode);
public:
  FGNavRecord(PositionedID aGuid, Type type, const std::string& ident,
              const std::string& name,
              const SGGeod& aPos,
              int freq, int range, double multiuse,
              PositionedID aRunway);
    
    inline double get_lon() const { return longitude(); } // degrees
    inline double get_lat() const { return latitude(); } // degrees
    inline double get_elev_ft() const { return elevation(); }
        
    inline int get_freq() const { return freq; }
    inline int get_range() const { return range; }
    inline double get_multiuse() const { return multiuse; }
    inline void set_multiuse( double m ) { multiuse = m; }
    inline const char *get_ident() const { return ident().c_str(); }

    inline bool get_serviceable() const { return serviceable; }
    inline const char *get_trans_ident() const { return get_ident(); }

  virtual const std::string& name() const
  { return mName; }
  
  /**
   * Retrieve the runway this navaid is associated with (for ILS/LOC/GS)
   */
  FGRunwayRef runway() const;
  
  /**
   * return the localizer width, in degrees
   * computation is based up ICAO stdandard width at the runway threshold
   * see implementation for further details.
   */
  double localizerWidth() const;
  
  void bindToNode(SGPropertyNode* nd) const;
  void unbindFromNode(SGPropertyNode* nd) const;

  void setColocatedDME(PositionedID other);
  bool hasDME();
};

class FGTACANRecord : public SGReferenced {

    std::string channel;		
    int freq;
     
public:
    
     FGTACANRecord(void);
    inline ~FGTACANRecord(void) {}

    inline const std::string& get_channel() const { return channel; }
    inline int get_freq() const { return freq; }
    friend std::istream& operator>> ( std::istream&, FGTACANRecord& );
    };

#endif // _FG_NAVRECORD_HXX
