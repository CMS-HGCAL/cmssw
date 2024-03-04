#ifndef hgcal_slinkfromraw_RecordStopping_h
#define hgcal_slinkfromraw_RecordStopping_h

#include <iostream>
#include <iomanip>
#include <cassert>

#include "Record.h"

namespace hgcal_slinkfromraw {

  class RecordStopping : public RecordT<3> {
  
  public:
    RecordStopping() {
    }
    
    bool valid() const {
      return validPattern() && state()==FsmState::Stopping;
    }
    
    void setHeader(uint32_t t=time(0)) {
      setState(FsmState::Stopping);
      setPayloadLength(3);
      setUtc(t);
    }

    uint32_t runNumber() const {
      return _payload[0]&0xffffffff;
    }
    uint32_t numberOfEvents() const {
      return _payload[0]>>32;
    }

    uint32_t numberOfSeconds() const {
      return _payload[1]&0xffffffff;
    }

    uint32_t numberOfSpills() const {
      return _payload[1]>>32;
    }

    uint32_t numberOfPauses() const {
      return _payload[2]&0xffffffff;
    }

    void setRunNumber(uint32_t t=time(0)) {
      _payload[0]&=0xffffffff00000000;
      _payload[0]|=t;
    }
   
    void setNumberOfEvents(uint32_t e) {
      _payload[0]&=0x00000000ffffffff;
      _payload[0]|=uint64_t(e)<<32;
    }
   
    void setNumberOfSeconds(uint32_t s) {
      _payload[1]&=0xffffffff00000000;
      _payload[1]|=s;
    }
   
    void setNumberOfSpills(uint32_t s) {
      _payload[1]&=0x00000000ffffffff;
      _payload[1]|=uint64_t(s)<<32;
    }
   
    void setNumberOfPauses(uint32_t p) {
      //_payload[2]&=0xffffffff00000000;
      _payload[2]=0xdeaddead00000000; // Until upper slot filled
      _payload[2]|=p;
    }
   
    void print(std::ostream &o=std::cout, std::string s="") const {
      o << s << "RecordStopping::print()" << std::endl;
      RecordHeader::print(o,s+" ");
      
      for(unsigned i(0);i<payloadLength();i++) {
	o << s << "   Payload word " << std::setw(5) << " = 0x"
	  << std::hex << std::setfill('0')
	  << std::setw(16) << _payload[i]
	  << std::dec << std::setfill(' ') << std::endl;
      }
      
      o << s << "  Run number        = "
	<< std::setw(10) << runNumber() << std::endl;
      o << s << "  Number of events  = "
	<< std::setw(10) << numberOfEvents() << std::endl;
      o << s << "  Number of seconds = "
	<< std::setw(10) << numberOfSeconds() << std::endl;
      o << s << "  Number of spills  = "
	<< std::setw(10) << numberOfSpills() << std::endl;
      o << s << "  Number of pauses  = "
	<< std::setw(10) << numberOfPauses() << std::endl;
    }

  private:
  };

}

#endif
