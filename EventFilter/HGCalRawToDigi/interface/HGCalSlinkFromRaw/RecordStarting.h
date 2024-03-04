#ifndef hgcal_slinkfromraw_RecordStarting_h
#define hgcal_slinkfromraw_RecordStarting_h

#include <iostream>
#include <iomanip>
#include <cassert>

#include "Record.h"

namespace hgcal_slinkfromraw {

  class RecordStarting : public RecordT<2> {
  
  public:
    RecordStarting() {
    }
    
    bool valid() const {
      return validPattern() && state()==FsmState::Starting;
    }
    
    void setHeader(uint32_t t=time(0)) {
      setState(FsmState::Starting);
      setPayloadLength(2);
      setUtc(t);
    }

    uint32_t runNumber() const {
      return _payload[0]&0xffffffff;
    }
    uint32_t maxEvents() const {
      return _payload[0]>>32;
    }

    uint32_t maxSeconds() const {
      return _payload[1]&0xffffffff;
    }

    uint32_t maxSpills() const {
      return _payload[1]>>32;
    }

    void setRunNumber(uint32_t t=time(0)) {
      _payload[0]&=0xffffffff00000000;
      _payload[0]|=t;
    }
   
    void setMaxEvents(uint32_t e) {
      _payload[0]&=0x00000000ffffffff;
      _payload[0]|=uint64_t(e)<<32;
    }
   
    void setMaxSeconds(uint32_t s) {
      _payload[1]&=0xffffffff00000000;
      _payload[1]|=s;
    }
   
    void setMaxSpills(uint32_t s) {
      _payload[1]&=0x00000000ffffffff;
      _payload[1]|=uint64_t(s)<<32;
    }
   
    void print(std::ostream &o=std::cout, std::string s="") const {
      o << s << "RecordStarting::print()" << std::endl;
      RecordHeader::print(o,s+" ");
      
      for(unsigned i(0);i<payloadLength();i++) {
	o << s << "   Payload word " << std::setw(5) << " = 0x"
	  << std::hex << std::setfill('0')
	  << std::setw(16) << _payload[i]
	  << std::dec << std::setfill(' ') << std::endl;
      }
      o << s << "  Run number                = "
	<< std::setw(10) << runNumber() << std::endl;
      o << s << "  Maximum number of events  = "
	<< std::setw(10) << maxEvents() << std::endl;
      o << s << "  Maximum number of seconds = "
	<< std::setw(10) << maxSeconds() << std::endl;
      o << s << "  Maximum number of spills  = "
	<< std::setw(10) << maxSpills() << std::endl;
    }
  private:
  };

}

#endif
