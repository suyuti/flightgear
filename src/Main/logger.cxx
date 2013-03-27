// logger.cxx - log properties.
// Written by David Megginson, started 2002.
//
// This file is in the Public Domain, and comes with no warranty.

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "logger.hxx"

#include <fstream>
#include <string>

#include <simgear/debug/logstream.hxx>

#include "fg_props.hxx"

using std::string;
using std::endl;

////////////////////////////////////////////////////////////////////////
// Implementation of FGLogger
////////////////////////////////////////////////////////////////////////

FGLogger::FGLogger ()
{
}

FGLogger::~FGLogger ()
{
}

void
FGLogger::init ()
{
  SGPropertyNode * logging = fgGetNode("/logging");
  if (logging == 0)
    return;

  std::vector<SGPropertyNode_ptr> children = logging->getChildren("log");
  for (unsigned int i = 0; i < children.size(); i++) {

    SGPropertyNode * child = children[i];

    if (!child->getBoolValue("enabled", false))
        continue;

    _logs.push_back(Log());
    Log &log = _logs[_logs.size()-1];
    
    string filename = child->getStringValue("filename");
    if (filename.size() == 0) {
        filename = "fg_log.csv";
        child->setStringValue("filename", filename.c_str());
    }

    string delimiter = child->getStringValue("delimiter");
    if (delimiter.size() == 0) {
        delimiter = ",";
        child->setStringValue("delimiter", delimiter.c_str());
    }
        
    log.interval_ms = child->getLongValue("interval-ms");
    log.last_time_ms = globals->get_sim_time_sec() * 1000;
    log.delimiter = delimiter.c_str()[0];
    log.output = new std::ofstream(filename.c_str());
    if (!log.output) {
      SG_LOG(SG_GENERAL, SG_ALERT, "Cannot write log to " << filename);
      continue;
    }

    //
    // Process the individual entries (Time is automatic).
    //
    std::vector<SGPropertyNode_ptr> entries = child->getChildren("entry");
    (*log.output) << "Time";
    for (unsigned int j = 0; j < entries.size(); j++) {
      SGPropertyNode * entry = entries[j];

      //
      // Set up defaults.
      //
      if (!entry->hasValue("property")) {
          entry->setBoolValue("enabled", false);
          continue;
      }

      if (!entry->getBoolValue("enabled"))
          continue;

      SGPropertyNode * node =
	fgGetNode(entry->getStringValue("property"), true);
      log.nodes.push_back(node);
      (*log.output) << log.delimiter
		    << entry->getStringValue("title", node->getPath().c_str());
    }
    (*log.output) << endl;
  }
}

void
FGLogger::reinit ()
{
    _logs.clear();
    init();
}

void
FGLogger::bind ()
{
}

void
FGLogger::unbind ()
{
}

void
FGLogger::update (double dt)
{
    double sim_time_sec = globals->get_sim_time_sec();
    double sim_time_ms = sim_time_sec * 1000;
    for (unsigned int i = 0; i < _logs.size(); i++) {
        while ((sim_time_ms - _logs[i].last_time_ms) >= _logs[i].interval_ms) {
            _logs[i].last_time_ms += _logs[i].interval_ms;
            (*_logs[i].output) << sim_time_sec;
            for (unsigned int j = 0; j < _logs[i].nodes.size(); j++) {
                (*_logs[i].output) << _logs[i].delimiter
			   << _logs[i].nodes[j]->getStringValue();
            }
            (*_logs[i].output) << endl;
        }
    }
}



////////////////////////////////////////////////////////////////////////
// Implementation of FGLogger::Log
////////////////////////////////////////////////////////////////////////

FGLogger::Log::Log ()
  : output(0),
    interval_ms(0),
    last_time_ms(-999999.0),
    delimiter(',')
{
}

FGLogger::Log::~Log ()
{
  delete output;
}

// end of logger.cxx
