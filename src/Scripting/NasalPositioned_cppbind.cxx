// NasalPositioned_cppbind.cxx -- expose FGPositioned classes to Nasal
//
// Port of NasalPositioned.cpp to the new nasal/cppbind helpers. Will replace
// old NasalPositioned.cpp once finished.
//
// Copyright (C) 2013  Thomas Geymayer <tomgey@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "NasalPositioned.hxx"

#include <algorithm>
#include <functional>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <simgear/nasal/cppbind/from_nasal.hxx>
#include <simgear/nasal/cppbind/to_nasal.hxx>
#include <simgear/nasal/cppbind/NasalHash.hxx>
#include <simgear/nasal/cppbind/Ghost.hxx>

#include <Airports/airport.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/pavement.hxx>
#include <ATC/CommStation.hxx>
#include <Main/globals.hxx>
#include <Navaids/NavDataCache.hxx>
#include <Navaids/navlist.hxx>
#include <Navaids/navrecord.hxx>
#include <Navaids/fix.hxx>

typedef nasal::Ghost<FGPositionedRef> NasalPositioned;
typedef nasal::Ghost<FGRunwayRef> NasalRunway;
typedef nasal::Ghost<FGParkingRef> NasalParking;
typedef nasal::Ghost<FGAirportRef> NasalAirport;
typedef nasal::Ghost<flightgear::CommStationRef> NasalCommStation;
typedef nasal::Ghost<FGNavRecordRef> NasalNavRecord;
typedef nasal::Ghost<FGRunwayRef> NasalRunway;
typedef nasal::Ghost<FGFixRef> NasalFix;

//------------------------------------------------------------------------------
naRef to_nasal_helper(naContext c, flightgear::SID* sid)
{
  // TODO SID ghost
  return nasal::to_nasal(c, sid->ident());
}

//------------------------------------------------------------------------------
naRef to_nasal_helper(naContext c, flightgear::STAR* star)
{
  // TODO STAR ghost
  return nasal::to_nasal(c, star->ident());
}

//------------------------------------------------------------------------------
naRef to_nasal_helper(naContext c, flightgear::Approach* iap)
{
  // TODO Approach ghost
  return nasal::to_nasal(c, iap->ident());
}

//------------------------------------------------------------------------------
static naRef f_navaid_course(naContext, FGNavRecord& nav)
{
  if( !(  nav.type() == FGPositioned::ILS
       || nav.type() == FGPositioned::LOC
       ) )
    return naNil();

  double radial = nav.get_multiuse();
  return naNum(SGMiscd::normalizePeriodic(0.5, 360.5, radial));
}

//------------------------------------------------------------------------------
static FGRunwayBaseRef f_airport_runway(FGAirport& apt, std::string ident)
{
  boost::to_upper(ident);

  if( apt.hasRunwayWithIdent(ident) )
    return apt.getRunwayByIdent(ident);
  else if( apt.hasHelipadWithIdent(ident) )
    return apt.getHelipadByIdent(ident);

  return 0;
}

//------------------------------------------------------------------------------
template<class T, class C1, class C2>
std::vector<T> extract( const std::vector<C1>& in,
                        T (C2::*getter)() const )
{
  std::vector<T> ret(in.size());
  std::transform(in.begin(), in.end(), ret.begin(), std::mem_fun(getter));
  return ret;
}

//------------------------------------------------------------------------------
static naRef f_airport_comms(FGAirport& apt, const nasal::CallContext& ctx)
{
  FGPositioned::Type comm_type =
    FGPositioned::typeFromName( ctx.getArg<std::string>(0) );

  // if we have an explicit type, return a simple vector of frequencies
  if( comm_type != FGPositioned::INVALID )
    return ctx.to_nasal
    (
      extract( apt.commStationsOfType(comm_type),
               &flightgear::CommStation::freqMHz )
    );
  else
    // otherwise return a vector of ghosts, one for each comm station.
    return ctx.to_nasal(apt.commStations());
}

//------------------------------------------------------------------------------
FGRunway* runwayFromNasalArg( const FGAirport& apt,
                              const nasal::CallContext& ctx,
                              size_t index = 0 )
{
  if( index >= ctx.argc )
    return NULL;

  try
  {
    std::string ident = ctx.getArg<std::string>(index);
    if( !ident.empty() )
    {
      if( !apt.hasRunwayWithIdent(ident) )
        // TODO warning/exception?
        return NULL;

      return apt.getRunwayByIdent(ident);
    }
  }
  catch(...)
  {}

  // TODO warn/error if no runway?
  return NasalRunway::fromNasal(ctx.c, ctx.args[index]);
}

//------------------------------------------------------------------------------
static naRef f_airport_sids(FGAirport& apt, const nasal::CallContext& ctx)
{
  FGRunway* rwy = runwayFromNasalArg(apt, ctx);
  return ctx.to_nasal
  (
    extract(rwy ? rwy->getSIDs() : apt.getSIDs(), &flightgear::SID::ident)
  );
}

//------------------------------------------------------------------------------
static naRef f_airport_stars(FGAirport& apt, const nasal::CallContext& ctx)
{
  FGRunway* rwy = runwayFromNasalArg(apt, ctx);
  return ctx.to_nasal
  (
    extract(rwy ? rwy->getSTARs() : apt.getSTARs(), &flightgear::STAR::ident)
  );
}

//------------------------------------------------------------------------------
static naRef f_airport_approaches(FGAirport& apt, const nasal::CallContext& ctx)
{
  FGRunway* rwy = runwayFromNasalArg(apt, ctx);

  flightgear::ProcedureType type = flightgear::PROCEDURE_INVALID;
  std::string type_str = ctx.getArg<std::string>(1);
  if( !type_str.empty() )
  {
    boost::to_upper(type_str);
    if(      type_str == "NDB" ) type = flightgear::PROCEDURE_APPROACH_NDB;
    else if( type_str == "VOR" ) type = flightgear::PROCEDURE_APPROACH_VOR;
    else if( type_str == "ILS" ) type = flightgear::PROCEDURE_APPROACH_ILS;
    else if( type_str == "RNAV") type = flightgear::PROCEDURE_APPROACH_RNAV;
  }

  return ctx.to_nasal
  (
    extract( rwy ? rwy->getApproaches(type)
                 // no runway specified, report them all
                 : apt.getApproaches(type),
             &flightgear::Approach::ident )
  );
}

//------------------------------------------------------------------------------
static FGParkingList
f_airport_parking(FGAirport& apt, nasal::CallContext ctx)
{
  std::string type = ctx.getArg<std::string>(0);
  bool only_available = ctx.getArg<bool>(1);

  FGAirportDynamics* dynamics = apt.getDynamics();
  PositionedIDVec parkings =
    flightgear::NavDataCache::instance()
      ->airportItemsOfType(apt.guid(), FGPositioned::PARKING);

  FGParkingList ret;
  BOOST_FOREACH(PositionedID parking, parkings)
  {
    // filter out based on availability and type
    if( only_available && !dynamics->isParkingAvailable(parking) )
      continue;

    FGParking* park = dynamics->getParking(parking);
    if( !type.empty() && (park->getType() != type) )
      continue;

    ret.push_back(park);
  }

  return ret;
}

/**
 * Extract a SGGeod from a nasal function argument list.
 *
 * <lat>, <lon>
 * {"lat": <lat-deg>, "lon": <lon-deg>}
 * geo.Coord.new() (aka. {"_lat": <lat-rad>, "_lon": <lon-rad>})
 */
static bool extractGeod(nasal::CallContext& ctx, SGGeod& result)
{
  if( !ctx.argc )
    return false;

  if( ctx.isGhost(0) )
  {
    FGPositioned* pos =
      NasalPositioned::fromNasal(ctx.c, ctx.requireArg<naRef>(0));

    if( pos )
    {
      result = pos->geod();
      ctx.popFront();
      return true;
    }
  }
  else if( ctx.isHash(0) )
  {
    nasal::Hash pos_hash = ctx.requireArg<nasal::Hash>(0);

    // check for manual latitude / longitude names
    naRef lat = pos_hash.get("lat"),
          lon = pos_hash.get("lon");
    if( naIsNum(lat) && naIsNum(lon) )
    {
      result = SGGeod::fromDeg( ctx.from_nasal<double>(lon),
                                ctx.from_nasal<double>(lat) );
      ctx.popFront();
      return true;
    }

    // geo.Coord uses _lat/_lon in radians
    // TODO should we check if its really a geo.Coord?
    lat = pos_hash.get("_lat");
    lon = pos_hash.get("_lon");
    if( naIsNum(lat) && naIsNum(lon) )
    {
      result = SGGeod::fromRad( ctx.from_nasal<double>(lon),
                                ctx.from_nasal<double>(lat) );
      ctx.popFront();
      return true;
    }
  }
  else if( ctx.isNumeric(0) && ctx.isNumeric(1) )
  {
    // lat, lon
    result = SGGeod::fromDeg( ctx.requireArg<double>(1),
                              ctx.requireArg<double>(0) );
    ctx.popFront(2);
    return true;
  }

  return false;
}

/**
 * Extract position from ctx or return current aircraft position if not given.
 */
static SGGeod getPosition(nasal::CallContext& ctx)
{
  SGGeod pos;
  if( !extractGeod(ctx, pos) )
    pos = globals->get_aircraft_position();

  return pos;
}

//------------------------------------------------------------------------------
// Returns Nasal ghost for particular or nearest airport of a <type>, or nil
// on error.
//
// airportinfo(<id>);                   e.g. "KSFO"
// airportinfo(<type>);                 type := ("airport"|"seaport"|"heliport")
// airportinfo()                        same as  airportinfo("airport")
// airportinfo(<lat>, <lon> [, <type>]);
static naRef f_airportinfo(nasal::CallContext ctx)
{
  SGGeod pos = getPosition(ctx);

  if( ctx.argc > 1 )
    naRuntimeError(ctx.c, "airportinfo() with invalid function arguments");

  // optional type/ident
  std::string ident("airport");
  if( ctx.isString(0) )
    ident = ctx.requireArg<std::string>(0);

  FGAirport::TypeRunwayFilter filter;
  if( !filter.fromTypeString(ident) )
    // user provided an <id>, hopefully
    return ctx.to_nasal(FGAirport::findByIdent(ident));

  double maxRange = 10000.0; // expose this? or pick a smaller value?
  return ctx.to_nasal( FGAirport::findClosest(pos, maxRange, &filter) );
}

/**
 * findAirportsWithinRange([<position>,] <range-nm> [, type])
 */
static naRef f_findAirportsWithinRange(nasal::CallContext ctx)
{
  SGGeod pos = getPosition(ctx);
  double range_nm = ctx.requireArg<double>(0);

  FGAirport::TypeRunwayFilter filter; // defaults to airports only
  filter.fromTypeString( ctx.getArg<std::string>(1) );

  FGPositionedList apts = FGPositioned::findWithinRange(pos, range_nm, &filter);
  FGPositioned::sortByRange(apts, pos);

  return ctx.to_nasal(apts);
}

/**
 * findAirportsByICAO(<ident/prefix> [, type])
 */
static naRef f_findAirportsByICAO(nasal::CallContext ctx)
{
  std::string prefix = ctx.requireArg<std::string>(0);

  FGAirport::TypeRunwayFilter filter; // defaults to airports only
  filter.fromTypeString( ctx.getArg<std::string>(1) );

  return ctx.to_nasal( FGPositioned::findAllWithIdent(prefix, &filter, false) );
}

// Returns vector of data hash for navaid of a <type>, nil on error
// navaids sorted by ascending distance
// navinfo([<lat>,<lon>],[<type>],[<id>])
// lat/lon (numeric): use latitude/longitude instead of ac position
// type:              ("fix"|"vor"|"ndb"|"ils"|"dme"|"tacan"|"any")
// id:                (partial) id of the fix
// examples:
// navinfo("vor")     returns all vors
// navinfo("HAM")     return all navaids who's name start with "HAM"
// navinfo("vor", "HAM") return all vor who's name start with "HAM"
//navinfo(34,48,"vor","HAM") return all vor who's name start with "HAM"
//                           sorted by distance relative to lat=34, lon=48
static naRef f_navinfo(nasal::CallContext ctx)
{
  SGGeod pos = getPosition(ctx);
  std::string id = ctx.getArg<std::string>(0);

  FGNavList::TypeFilter filter;
  if( filter.fromTypeString(id) )
    id = ctx.getArg<std::string>(1);
  else if( ctx.argc > 1 )
    naRuntimeError(ctx.c, "navinfo() already got an ident");

  return ctx.to_nasal( FGNavList::findByIdentAndFreq(pos, id, 0.0, &filter) );
}

//------------------------------------------------------------------------------
static naRef f_findWithinRange(nasal::CallContext ctx)
{
  SGGeod pos = getPosition(ctx);
  double range_nm = ctx.requireArg<double>(0);

  FGPositioned::TypeFilter filter(FGPositioned::typeFromName(ctx.getArg<std::string>(1)));
    
  FGPositionedList items = FGPositioned::findWithinRange(pos, range_nm, &filter);
  FGPositioned::sortByRange(items, pos);
  return ctx.to_nasal(items);
}

static naRef f_findByIdent(nasal::CallContext ctx)
{
  std::string prefix = ctx.requireArg<std::string>(0);
  
  FGPositioned::TypeFilter filter(FGPositioned::typeFromName(ctx.getArg<std::string>(1)));
  bool exact = ctx.getArg<bool>(2, false);

  return ctx.to_nasal( FGPositioned::findAllWithIdent(prefix, &filter, exact) );
}

static naRef f_findByName(nasal::CallContext ctx)
{
  std::string prefix = ctx.requireArg<std::string>(0);
  
  FGPositioned::TypeFilter filter(FGPositioned::typeFromName(ctx.getArg<std::string>(1)));
  
  return ctx.to_nasal( FGPositioned::findAllWithName(prefix, &filter, false) );
}

//------------------------------------------------------------------------------

static naRef f_courseAndDistance(nasal::CallContext ctx)
{
  SGGeod from = globals->get_aircraft_position(), to, pos;
  bool ok = extractGeod(ctx, pos);
  if (!ok) {
    naRuntimeError(ctx.c, "invalid arguments to courseAndDistance");
  }
  
  if (extractGeod(ctx, to)) {
    from = pos; // we parsed both FROM and TO args, so first was FROM
  } else {
    to = pos; // only parsed one arg, so FROM is current
  }
  
  double course, course2, d;
  SGGeodesy::inverse(from, to, course, course2, d);
  
  naRef result = naNewVector(ctx.c);
  naVec_append(result, naNum(course));
  naVec_append(result, naNum(d * SG_METER_TO_NM));
  return result;
}

static naRef f_sortByRange(nasal::CallContext ctx)
{
  FGPositionedList items = ctx.requireArg<FGPositionedList>(0);
  ctx.popFront();
  FGPositioned::sortByRange(items, getPosition(ctx));
  return ctx.to_nasal(items);
}

//------------------------------------------------------------------------------
naRef initNasalPositioned_cppbind(naRef globalsRef, naContext c, naRef gcSave)
{
  NasalPositioned::init("Positioned")
    .member("id", &FGPositioned::ident)
    .member("ident", &FGPositioned::ident) // TODO to we really need id and ident?
    .member("name", &FGPositioned::name)
    .member("type", &FGPositioned::typeString)
    .member("lat", &FGPositioned::latitude)
    .member("lon", &FGPositioned::longitude)
    .member("elevation", &FGPositioned::elevationM);
  NasalRunway::init("Runway")
    .bases<NasalPositioned>();
  NasalParking::init("Parking")
    .bases<NasalPositioned>();
  NasalCommStation::init("CommStation")
    .bases<NasalPositioned>()
    .member("frequency", &flightgear::CommStation::freqMHz);
  NasalNavRecord::init("Navaid")
    .bases<NasalPositioned>()
    .member("frequency", &FGNavRecord::get_freq)
    .member("range_nm", &FGNavRecord::get_range)
    .member("course", &f_navaid_course);

  NasalFix::init("Fix")
    .bases<NasalPositioned>();
  
  NasalAirport::init("FGAirport")
    .bases<NasalPositioned>()
    .member("has_metar", &FGAirport::getMetar)
    .member("runways", &FGAirport::getRunwayMap)
    .member("helipads", &FGAirport::getHelipadMap)
    .member("taxiways", &FGAirport::getTaxiways)
    .member("pavements", &FGAirport::getPavements)
    .method("runway", &f_airport_runway)
    .method("helipad", &f_airport_runway)
    .method("tower", &FGAirport::getTowerLocation)
    .method("comms", &f_airport_comms)
    .method("sids", &f_airport_sids)
    .method("stars", &f_airport_stars)
    .method("getApproachList", f_airport_approaches)
    .method("parking", &f_airport_parking)
    .method("getSid", &FGAirport::findSIDWithIdent)
    .method("getStar", &FGAirport::findSTARWithIdent)
    .method("getIAP", &FGAirport::findApproachWithIdent)
    .method("tostring", &FGAirport::toString);

  nasal::Hash globals(globalsRef, c),
              positioned( globals.createHash("positioned") );

  positioned.set("airportinfo", &f_airportinfo);
  positioned.set("findAirportsWithinRange", f_findAirportsWithinRange);
  positioned.set("findAirportsByICAO", &f_findAirportsByICAO);
  positioned.set("navinfo", &f_navinfo);
  
  positioned.set("findWithinRange", &f_findWithinRange);
  positioned.set("findByIdent", &f_findByIdent);
  positioned.set("findByName", &f_findByName);
  positioned.set("courseAndDistance", &f_courseAndDistance);
  positioned.set("sortByRange", &f_sortByRange);
  
  return naNil();
}
