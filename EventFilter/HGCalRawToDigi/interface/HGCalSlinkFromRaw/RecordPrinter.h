#ifndef hgcal_slinkfromraw_RecordPrinter_h
#define hgcal_slinkfromraw_RecordPrinter_h

#include <iostream>
#include <iomanip>
#include <cassert>

#include "RecordRunning.h"
#include "RecordStarting.h"
#include "RecordStopping.h"
#include "RecordContinuing.h"

namespace hgcal_slinkfromraw {

  void RecordPrinter(const Record *h, std::ostream &o=std::cout, std::string s="") {
    if(h->state()==FsmState::Running      ) ((const RecordRunning* )h)->print(o,s);
    else if(h->state()==FsmState::Starting) ((const RecordStarting*)h)->print(o,s);
    else if(h->state()==FsmState::Stopping) ((const RecordStopping*)h)->print(o,s);
    else h->print(o,s);
  }

  void RecordPrinter(const Record &h, std::ostream &o=std::cout, std::string s="") {
    RecordPrinter(&h,o,s);
  }
}

#endif
