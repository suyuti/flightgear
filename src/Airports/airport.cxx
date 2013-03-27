//
// simple.cxx -- a really simplistic class to manage airport ID,
//               lat, lon of the center of one of it's runways, and
//               elevation in feet.
//
// Written by Curtis Olson, started April 1998.
// Updated by Durk Talsma, started December, 2004.
//
// Copyright (C) 1998  Curtis L. Olson  - http://www.flightgear.org/~curt
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "airport.hxx"

#include <algorithm>
#include <cassert>
#include <boost/foreach.hpp>

#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/sg_inlines.h>
#include <simgear/structure/exception.hxx>

#include <Environment/environment_mgr.hxx>
#include <Environment/environment.hxx>
#include <Main/fg_props.hxx>
#include <Airports/runways.hxx>
#include <Airports/pavement.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/xmlloader.hxx>
#include <Navaids/procedure.hxx>
#include <Navaids/waypoint.hxx>
#include <ATC/CommStation.hxx>
#include <Navaids/NavDataCache.hxx>

using std::vector;
using std::pair;

using namespace flightgear;

/***************************************************************************
 * FGAirport
 ***************************************************************************/

AirportCache FGAirport::airportCache;

FGAirport::FGAirport( PositionedID aGuid,
                      const std::string &id,
                      const SGGeod& location,
                      const std::string &name,
                      bool has_metar,
                      Type aType ):
    FGPositioned(aGuid, aType, id, location),
    _name(name),
    _has_metar(has_metar),
    _dynamics(0),
    mTowerDataLoaded(false),
    mRunwaysLoaded(false),
    mHelipadsLoaded(false),
    mTaxiwaysLoaded(false),
    mProceduresLoaded(false),
    mILSDataLoaded(false)
{
}


FGAirport::~FGAirport()
{
    delete _dynamics;
}

bool FGAirport::isAirport() const
{
  return type() == AIRPORT;
}

bool FGAirport::isSeaport() const
{
  return type() == SEAPORT;
}

bool FGAirport::isHeliport() const
{
  return type() == HELIPORT;
}

bool FGAirport::isAirportType(FGPositioned* pos)
{
    if (!pos) {
        return false;
    }
    
    return (pos->type() >= AIRPORT) && (pos->type() <= SEAPORT);
}

FGAirportDynamics * FGAirport::getDynamics()
{
    if (_dynamics) {
        return _dynamics;
    }
    
    _dynamics = new FGAirportDynamics(this);
    XMLLoader::load(_dynamics);
    _dynamics->init();
  
    FGRunwayPreference rwyPrefs(this);
    XMLLoader::load(&rwyPrefs);
    _dynamics->setRwyUse(rwyPrefs);
    
    return _dynamics;
}

//------------------------------------------------------------------------------
unsigned int FGAirport::numRunways() const
{
  loadRunways();
  return mRunways.size();
}

//------------------------------------------------------------------------------
unsigned int FGAirport::numHelipads() const
{
  loadHelipads();
  return mHelipads.size();
}

//------------------------------------------------------------------------------
FGRunwayRef FGAirport::getRunwayByIndex(unsigned int aIndex) const
{
  loadRunways();
  return loadById<FGRunway>(mRunways, aIndex);
}

//------------------------------------------------------------------------------
FGHelipadRef FGAirport::getHelipadByIndex(unsigned int aIndex) const
{
  loadHelipads();
  return loadById<FGHelipad>(mHelipads, aIndex);
}

//------------------------------------------------------------------------------
FGRunwayMap FGAirport::getRunwayMap() const
{
  loadRunways();
  FGRunwayMap map;

  double minLengthFt = fgGetDouble("/sim/navdb/min-runway-length-ft");

  BOOST_FOREACH(PositionedID id, mRunways)
  {
    FGRunway* rwy = loadById<FGRunway>(id);

    // ignore unusably short runways
    // TODO other methods don't check this...
    if( rwy->lengthFt() >= minLengthFt )
      map[ rwy->ident() ] = rwy;
  }

  return map;
}

//------------------------------------------------------------------------------
FGHelipadMap FGAirport::getHelipadMap() const
{
  loadHelipads();
  FGHelipadMap map;

  BOOST_FOREACH(PositionedID id, mHelipads)
  {
    FGHelipad* rwy = loadById<FGHelipad>(id);
    map[ rwy->ident() ] = rwy;
  }

  return map;
}

//------------------------------------------------------------------------------
bool FGAirport::hasRunwayWithIdent(const std::string& aIdent) const
{
  return flightgear::NavDataCache::instance()
    ->airportItemWithIdent(guid(), FGPositioned::RUNWAY, aIdent) != 0;
}

//------------------------------------------------------------------------------
bool FGAirport::hasHelipadWithIdent(const std::string& aIdent) const
{
  return flightgear::NavDataCache::instance()
    ->airportItemWithIdent(guid(), FGPositioned::HELIPAD, aIdent) != 0;
}

//------------------------------------------------------------------------------
FGRunwayRef FGAirport::getRunwayByIdent(const std::string& aIdent) const
{
  PositionedID id =
    flightgear::NavDataCache::instance()
      ->airportItemWithIdent(guid(), FGPositioned::RUNWAY, aIdent);

  if (id == 0) {
    SG_LOG(SG_GENERAL, SG_ALERT, "no such runway '" << aIdent << "' at airport " << ident());
    throw sg_range_exception("unknown runway " + aIdent + " at airport:" + ident(), "FGAirport::getRunwayByIdent");
  }
  
  return loadById<FGRunway>(id);
}

//------------------------------------------------------------------------------
FGHelipadRef FGAirport::getHelipadByIdent(const std::string& aIdent) const
{
  PositionedID id = flightgear::NavDataCache::instance()->airportItemWithIdent(guid(), FGPositioned::HELIPAD, aIdent);
  if (id == 0) {
    SG_LOG(SG_GENERAL, SG_ALERT, "no such helipad '" << aIdent << "' at airport " << ident());
    throw sg_range_exception("unknown helipad " + aIdent + " at airport:" + ident(), "FGAirport::getRunwayByIdent");
  }

  return loadById<FGHelipad>(id);
}

//------------------------------------------------------------------------------
FGRunwayRef FGAirport::findBestRunwayForHeading(double aHeading) const
{
  loadRunways();
  
  FGRunway* result = NULL;
  double currentBestQuality = 0.0;
  
  SGPropertyNode *param = fgGetNode("/sim/airport/runways/search", true);
  double lengthWeight = param->getDoubleValue("length-weight", 0.01);
  double widthWeight = param->getDoubleValue("width-weight", 0.01);
  double surfaceWeight = param->getDoubleValue("surface-weight", 10);
  double deviationWeight = param->getDoubleValue("deviation-weight", 1);
    
  BOOST_FOREACH(PositionedID id, mRunways) {
    FGRunway* rwy = loadById<FGRunway>(id);
    double good = rwy->score(lengthWeight, widthWeight, surfaceWeight);
    double dev = aHeading - rwy->headingDeg();
    SG_NORMALIZE_RANGE(dev, -180.0, 180.0);
    double bad = fabs(deviationWeight * dev) + 1e-20;
    double quality = good / bad;
    
    if (quality > currentBestQuality) {
      currentBestQuality = quality;
      result = rwy;
    }
  }

  return result;
}

//------------------------------------------------------------------------------
FGRunwayRef FGAirport::findBestRunwayForPos(const SGGeod& aPos) const
{
  loadRunways();
  
  FGRunway* result = NULL;
  double currentLowestDev = 180.0;
  
  BOOST_FOREACH(PositionedID id, mRunways) {
    FGRunway* rwy = loadById<FGRunway>(id);

    double inboundCourse = SGGeodesy::courseDeg(aPos, rwy->end());
    double dev = inboundCourse - rwy->headingDeg();
    SG_NORMALIZE_RANGE(dev, -180.0, 180.0);

    dev = fabs(dev);
    if (dev < currentLowestDev) { // new best match
      currentLowestDev = dev;
      result = rwy;
    }
  } // of runway iteration
  
  return result;

}

//------------------------------------------------------------------------------
bool FGAirport::hasHardRunwayOfLengthFt(double aLengthFt) const
{
  loadRunways();
  
  BOOST_FOREACH(PositionedID id, mRunways) {
    FGRunway* rwy = loadById<FGRunway>(id);
    if (rwy->isHardSurface() && (rwy->lengthFt() >= aLengthFt)) {
      return true; // we're done!
    }
  } // of runways iteration

  return false;
}

//------------------------------------------------------------------------------
FGRunwayList FGAirport::getRunwaysWithoutReciprocals() const
{
  loadRunways();
  
  FGRunwayList r;
  
  BOOST_FOREACH(PositionedID id, mRunways) {
    FGRunway* rwy = loadById<FGRunway>(id);
    FGRunway* recip = rwy->reciprocalRunway();
    if (recip) {
      FGRunwayList::iterator it = std::find(r.begin(), r.end(), recip);
      if (it != r.end()) {
        continue; // reciprocal already in result set, don't include us
      }
    }
    
    r.push_back(rwy);
  }
  
  return r;
}

//------------------------------------------------------------------------------
unsigned int FGAirport::numTaxiways() const
{
  loadTaxiways();
  return mTaxiways.size();
}

//------------------------------------------------------------------------------
FGTaxiwayRef FGAirport::getTaxiwayByIndex(unsigned int aIndex) const
{
  loadTaxiways();
  return loadById<FGTaxiway>(mTaxiways, aIndex);
}

//------------------------------------------------------------------------------
FGTaxiwayList FGAirport::getTaxiways() const
{
  loadTaxiways();
  return loadAllById<FGTaxiway>(mTaxiways);
}

//------------------------------------------------------------------------------
unsigned int FGAirport::numPavements() const
{
  loadTaxiways();
  return mPavements.size();
}

//------------------------------------------------------------------------------
FGPavementRef FGAirport::getPavementByIndex(unsigned int aIndex) const
{
  loadTaxiways();
  return loadById<FGPavement>(mPavements, aIndex);
}

//------------------------------------------------------------------------------
FGPavementList FGAirport::getPavements() const
{
  loadTaxiways();
  return loadAllById<FGPavement>(mPavements);
}

//------------------------------------------------------------------------------
FGRunwayRef FGAirport::getActiveRunwayForUsage() const
{
  FGEnvironmentMgr* envMgr = (FGEnvironmentMgr *) globals->get_subsystem("environment");
  
  // This forces West-facing rwys to be used in no-wind situations
  // which is consistent with Flightgear's initial setup.
  double hdg = 270;
  
  if (envMgr) {
    FGEnvironment stationWeather(envMgr->getEnvironment(mPosition));
  
    double windSpeed = stationWeather.get_wind_speed_kt();
    if (windSpeed > 0.0) {
      hdg = stationWeather.get_wind_from_heading_deg();
    }
  }
  
  return findBestRunwayForHeading(hdg);
}

//------------------------------------------------------------------------------
FGAirportRef FGAirport::findClosest( const SGGeod& aPos,
                                     double aCuttofNm,
                                     Filter* filter )
{
  AirportFilter aptFilter;
  if( !filter )
    filter = &aptFilter;
  
  return static_pointer_cast<FGAirport>
  (
    FGPositioned::findClosest(aPos, aCuttofNm, filter)
  );
}

FGAirport::HardSurfaceFilter::HardSurfaceFilter(double minLengthFt) :
  mMinLengthFt(minLengthFt)
{
  if (minLengthFt < 0.0) {
    mMinLengthFt = fgGetDouble("/sim/navdb/min-runway-length-ft", 0.0);
  }
}

bool FGAirport::HardSurfaceFilter::passAirport(FGAirport* aApt) const
{
  return aApt->hasHardRunwayOfLengthFt(mMinLengthFt);
}

//------------------------------------------------------------------------------
FGAirport::TypeRunwayFilter::TypeRunwayFilter():
  _type(FGPositioned::AIRPORT),
  _min_runway_length_ft( fgGetDouble("/sim/navdb/min-runway-length-ft", 0.0) )
{

}

//------------------------------------------------------------------------------
bool FGAirport::TypeRunwayFilter::fromTypeString(const std::string& type)
{
  if(      type == "heliport" ) _type = FGPositioned::HELIPORT;
  else if( type == "seaport"  ) _type = FGPositioned::SEAPORT;
  else if( type == "airport"  ) _type = FGPositioned::AIRPORT;
  else                          return false;

  return true;
}

//------------------------------------------------------------------------------
bool FGAirport::TypeRunwayFilter::pass(FGPositioned* pos) const
{
  FGAirport* apt = static_cast<FGAirport*>(pos);
  if(  (apt->type() == FGPositioned::AIRPORT)
    && !apt->hasHardRunwayOfLengthFt(_min_runway_length_ft)
    )
    return false;

  return true;
}

//------------------------------------------------------------------------------
FGAirportRef FGAirport::findByIdent(const std::string& aIdent)
{
  AirportCache::iterator it = airportCache.find(aIdent);
  if (it != airportCache.end())
   return it->second;

  PortsFilter filter;
  FGAirportRef r = static_pointer_cast<FGAirport>
  (
    FGPositioned::findFirstWithIdent(aIdent, &filter)
  );

  // add airport to the cache (even when it's NULL, so we don't need to search in vain again)
  airportCache[aIdent] = r;

  // we don't warn here when r==NULL, let the caller do that
  return r;
}

//------------------------------------------------------------------------------
FGAirportRef FGAirport::getByIdent(const std::string& aIdent)
{
  FGAirportRef r = findByIdent(aIdent);
  if (!r)
    throw sg_range_exception("No such airport with ident: " + aIdent);
  return r;
}

char** FGAirport::searchNamesAndIdents(const std::string& aFilter)
{
  return NavDataCache::instance()->searchAirportNamesAndIdents(aFilter);
}

// find basic airport location info from airport database
const FGAirport *fgFindAirportID( const std::string& id)
{
    if ( id.empty() ) {
        return NULL;
    }
    
    return FGAirport::findByIdent(id);
}

void FGAirport::loadRunways() const
{
  if (mRunwaysLoaded) {
    return; // already loaded, great
  }
  
  loadSceneryDefinitions();
  
  mRunwaysLoaded = true;
  mRunways = flightgear::NavDataCache::instance()->airportItemsOfType(guid(), FGPositioned::RUNWAY);
}

void FGAirport::loadHelipads() const
{
  if (mHelipadsLoaded) {
    return; // already loaded, great
  }

  loadSceneryDefinitions();

  mHelipadsLoaded = true;
  mHelipads = flightgear::NavDataCache::instance()->airportItemsOfType(guid(), FGPositioned::HELIPAD);
}

void FGAirport::loadTaxiways() const
{
  if (mTaxiwaysLoaded) {
    return; // already loaded, great
  }
  
  mTaxiwaysLoaded =  true;
  mTaxiways = flightgear::NavDataCache::instance()->airportItemsOfType(guid(), FGPositioned::TAXIWAY);
}

void FGAirport::loadProcedures() const
{
  if (mProceduresLoaded) {
    return;
  }
  
  mProceduresLoaded = true;
  SGPath path;
  if (!XMLLoader::findAirportData(ident(), "procedures", path)) {
    SG_LOG(SG_GENERAL, SG_INFO, "no procedures data available for " << ident());
    return;
  }
  
  SG_LOG(SG_GENERAL, SG_INFO, ident() << ": loading procedures from " << path.str());
  RouteBase::loadAirportProcedures(path, const_cast<FGAirport*>(this));
}

void FGAirport::loadSceneryDefinitions() const
{
  NavDataCache* cache = NavDataCache::instance();
  SGPath path;
  if (!XMLLoader::findAirportData(ident(), "threshold", path)) {
    return; // no XML threshold data
  }
  
  if (!cache->isCachedFileModified(path)) {
    // cached values are correct, we're all done
    return;
  }
  
    flightgear::NavDataCache::Transaction txn(cache);
    SGPropertyNode_ptr rootNode = new SGPropertyNode;
    readProperties(path.str(), rootNode);
    const_cast<FGAirport*>(this)->readThresholdData(rootNode);
    cache->stampCacheFile(path);
    txn.commit();
}

void FGAirport::readThresholdData(SGPropertyNode* aRoot)
{
  SGPropertyNode* runway;
  int runwayIndex = 0;
  for (; (runway = aRoot->getChild("runway", runwayIndex)) != NULL; ++runwayIndex) {
    SGPropertyNode* t0 = runway->getChild("threshold", 0),
      *t1 = runway->getChild("threshold", 1);
    assert(t0);
    assert(t1); // too strict? maybe we should finally allow single-ended runways
    
    processThreshold(t0);
    processThreshold(t1);
  } // of runways iteration
}

void FGAirport::processThreshold(SGPropertyNode* aThreshold)
{
  // first, let's identify the current runway
  std::string rwyIdent(aThreshold->getStringValue("rwy"));
  NavDataCache* cache = NavDataCache::instance(); 
  PositionedID id = cache->airportItemWithIdent(guid(), FGPositioned::RUNWAY, rwyIdent);
  if (id == 0) {
    SG_LOG(SG_GENERAL, SG_DEBUG, "FGAirport::processThreshold: "
           "found runway not defined in the global data:" << ident() << "/" << rwyIdent);
    return;
  }
  
  double lon = aThreshold->getDoubleValue("lon"),
  lat = aThreshold->getDoubleValue("lat");
  SGGeod newThreshold(SGGeod::fromDegM(lon, lat, mPosition.getElevationM()));
  
  double newHeading = aThreshold->getDoubleValue("hdg-deg");
  double newDisplacedThreshold = aThreshold->getDoubleValue("displ-m");
  double newStopway = aThreshold->getDoubleValue("stopw-m");
  
  cache->updateRunwayThreshold(id, newThreshold,
                               newHeading, newDisplacedThreshold, newStopway);
}

SGGeod FGAirport::getTowerLocation() const
{
  validateTowerData();
  
  NavDataCache* cache = NavDataCache::instance();
  PositionedIDVec towers = cache->airportItemsOfType(guid(), FGPositioned::TOWER);
  if (towers.empty()) {
    SG_LOG(SG_GENERAL, SG_ALERT, "No towers defined for:" <<ident());
    return SGGeod();
  }
  
  FGPositionedRef tower = cache->loadById(towers.front());
  return tower->geod();
}

void FGAirport::validateTowerData() const
{
  if (mTowerDataLoaded) {
    return;
  }

  mTowerDataLoaded = true;
  NavDataCache* cache = NavDataCache::instance();
  SGPath path;
  if (!XMLLoader::findAirportData(ident(), "twr", path)) {
    return; // no XML tower data
  }
  
  if (!cache->isCachedFileModified(path)) {
  // cached values are correct, we're all done
    return;
  }
   
  flightgear::NavDataCache::Transaction txn(cache);
  SGPropertyNode_ptr rootNode = new SGPropertyNode;
  readProperties(path.str(), rootNode);
  const_cast<FGAirport*>(this)->readTowerData(rootNode);
  cache->stampCacheFile(path);
  txn.commit();
}

void FGAirport::readTowerData(SGPropertyNode* aRoot)
{
  SGPropertyNode* twrNode = aRoot->getChild("tower")->getChild("twr");
  double lat = twrNode->getDoubleValue("lat"), 
    lon = twrNode->getDoubleValue("lon"), 
    elevM = twrNode->getDoubleValue("elev-m");  
// tower elevation is AGL, not AMSL. Since we don't want to depend on the
// scenery for a precise terrain elevation, we use the field elevation
// (this is also what the apt.dat code does)
  double fieldElevationM = geod().getElevationM();
  SGGeod towerLocation(SGGeod::fromDegM(lon, lat, fieldElevationM + elevM));
  
  NavDataCache* cache = NavDataCache::instance();
  PositionedIDVec towers = cache->airportItemsOfType(guid(), FGPositioned::TOWER);
  if (towers.empty()) {
    cache->insertTower(guid(), towerLocation);
  } else {
    // update the position
    cache->updatePosition(towers.front(), towerLocation);
  }
}

bool FGAirport::validateILSData()
{
  if (mILSDataLoaded) {
    return false;
  }
  
  mILSDataLoaded = true;
  NavDataCache* cache = NavDataCache::instance();
  SGPath path;
  if (!XMLLoader::findAirportData(ident(), "ils", path)) {
    return false; // no XML tower data
  }
  
  if (!cache->isCachedFileModified(path)) {
    // cached values are correct, we're all done
    return false;
  }
  
  SGPropertyNode_ptr rootNode = new SGPropertyNode;
  readProperties(path.str(), rootNode);

  flightgear::NavDataCache::Transaction txn(cache);
  readILSData(rootNode);
  cache->stampCacheFile(path);
  txn.commit();
    
// we loaded data, tell the caller it might need to reload things
  return true;
}

void FGAirport::readILSData(SGPropertyNode* aRoot)
{
  NavDataCache* cache = NavDataCache::instance();
  
  // find the entry matching the runway
  SGPropertyNode* runwayNode, *ilsNode;
  for (int i=0; (runwayNode = aRoot->getChild("runway", i)) != NULL; ++i) {
    for (int j=0; (ilsNode = runwayNode->getChild("ils", j)) != NULL; ++j) {
      // must match on both nav-ident and runway ident, to support the following:
      // - runways with multiple distinct ILS installations (KEWD, for example)
      // - runways where both ends share the same nav ident (LFAT, for example)
      PositionedID ils = cache->findILS(guid(), ilsNode->getStringValue("rwy"),
                                        ilsNode->getStringValue("nav-id"));
      if (ils == 0) {
        SG_LOG(SG_GENERAL, SG_INFO, "reading ILS data for " << ident() <<
               ", couldn;t find runway/navaid for:" <<
               ilsNode->getStringValue("rwy") << "/" <<
               ilsNode->getStringValue("nav-id"));
        continue;
      }
      
      double hdgDeg = ilsNode->getDoubleValue("hdg-deg"),
        lon = ilsNode->getDoubleValue("lon"),
        lat = ilsNode->getDoubleValue("lat"),
        elevM = ilsNode->getDoubleValue("elev-m");
 
      cache->updateILS(ils, SGGeod::fromDegM(lon, lat, elevM), hdgDeg);
    } // of ILS iteration
  } // of runway iteration
}

void FGAirport::addSID(flightgear::SID* aSid)
{
  mSIDs.push_back(aSid);
}

void FGAirport::addSTAR(STAR* aStar)
{
  mSTARs.push_back(aStar);
}

void FGAirport::addApproach(Approach* aApp)
{
  mApproaches.push_back(aApp);
}

//------------------------------------------------------------------------------
unsigned int FGAirport::numSIDs() const
{
  loadProcedures();
  return mSIDs.size();
}

//------------------------------------------------------------------------------
flightgear::SID* FGAirport::getSIDByIndex(unsigned int aIndex) const
{
  loadProcedures();
  return mSIDs[aIndex];
}

//------------------------------------------------------------------------------
flightgear::SID* FGAirport::findSIDWithIdent(const std::string& aIdent) const
{
  loadProcedures();
  for (unsigned int i=0; i<mSIDs.size(); ++i) {
    if (mSIDs[i]->ident() == aIdent) {
      return mSIDs[i];
    }
  }
  
  return NULL;
}

//------------------------------------------------------------------------------
flightgear::SIDList FGAirport::getSIDs() const
{
  loadProcedures();
  return flightgear::SIDList(mSIDs.begin(), mSIDs.end());
}

//------------------------------------------------------------------------------
unsigned int FGAirport::numSTARs() const
{
  loadProcedures();
  return mSTARs.size();
}

//------------------------------------------------------------------------------
STAR* FGAirport::getSTARByIndex(unsigned int aIndex) const
{
  loadProcedures();
  return mSTARs[aIndex];
}

//------------------------------------------------------------------------------
STAR* FGAirport::findSTARWithIdent(const std::string& aIdent) const
{
  loadProcedures();
  for (unsigned int i=0; i<mSTARs.size(); ++i) {
    if (mSTARs[i]->ident() == aIdent) {
      return mSTARs[i];
    }
  }
  
  return NULL;
}

//------------------------------------------------------------------------------
STARList FGAirport::getSTARs() const
{
  loadProcedures();
  return STARList(mSTARs.begin(), mSTARs.end());
}

unsigned int FGAirport::numApproaches() const
{
  loadProcedures();
  return mApproaches.size();
}

//------------------------------------------------------------------------------
Approach* FGAirport::getApproachByIndex(unsigned int aIndex) const
{
  loadProcedures();
  return mApproaches[aIndex];
}

//------------------------------------------------------------------------------
Approach* FGAirport::findApproachWithIdent(const std::string& aIdent) const
{
  loadProcedures();
  for (unsigned int i=0; i<mApproaches.size(); ++i) {
    if (mApproaches[i]->ident() == aIdent) {
      return mApproaches[i];
    }
  }
  
  return NULL;
}

//------------------------------------------------------------------------------
ApproachList FGAirport::getApproaches(ProcedureType type) const
{
  loadProcedures();
  if( type == PROCEDURE_INVALID )
    return ApproachList(mApproaches.begin(), mApproaches.end());

  ApproachList ret;
  for(size_t i = 0; i < mApproaches.size(); ++i)
  {
    if( mApproaches[i]->type() == type )
      ret.push_back(mApproaches[i]);
  }
  return ret;
}

CommStationList
FGAirport::commStations() const
{
  NavDataCache* cache = NavDataCache::instance();
  CommStationList result;
  BOOST_FOREACH(PositionedID pos, cache->airportItemsOfType(guid(),
                                                            FGPositioned::FREQ_GROUND,
                                                            FGPositioned::FREQ_UNICOM))
  {
    result.push_back( loadById<CommStation>(pos) );
  }
  
  return result;
}

CommStationList
FGAirport::commStationsOfType(FGPositioned::Type aTy) const
{
  NavDataCache* cache = NavDataCache::instance();
  CommStationList result;
  BOOST_FOREACH(PositionedID pos, cache->airportItemsOfType(guid(), aTy)) {
    result.push_back( loadById<CommStation>(pos) );
  }
  
  return result;
}

// get airport elevation
double fgGetAirportElev( const std::string& id )
{
    const FGAirport *a=fgFindAirportID( id);
    if (a) {
        return a->getElevation();
    } else {
        return -9999.0;
    }
}


// get airport position
SGGeod fgGetAirportPos( const std::string& id )
{
    const FGAirport *a = fgFindAirportID( id);

    if (a) {
        return SGGeod::fromDegM(a->getLongitude(), a->getLatitude(), a->getElevation());
    } else {
        return SGGeod::fromDegM(0.0, 0.0, -9999.0);
    }
}
