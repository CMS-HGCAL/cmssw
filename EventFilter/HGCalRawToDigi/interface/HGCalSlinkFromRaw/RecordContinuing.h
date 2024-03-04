#ifndef Hgcal10gLinkReceiver_RecordContinuing_h
#define Hgcal10gLinkReceiver_RecordContinuing_h

#include <iostream>
#include <iomanip>
#include <cassert>

#include "Record.h"

namespace hgcal_slinkfromraw {

  class RecordContinuing : public RecordT<2> {
  
  public:
    RecordContinuing() {
    }
    
    void setHeader(uint32_t t=time(0)) {
      setState(FsmState::Continuing);
      setPayloadLength(2);
      setUtc(t);
    }

    bool valid() const {
      return validPattern() && state()==FsmState::Continuing;
    }
    
    uint32_t runNumber() const {
      return _payload[0]&0xffffffff;
    }
    
    uint32_t fileNumber() const {
      return _payload[0]>>32;
    }

    uint32_t numberOfEvents() const {
      return _payload[1]&0xffffffff;
    }

    uint32_t numberOfBytes() const {
      return _payload[1]>>32;
    }

    void setRunNumber(uint32_t t=time(0)) {
      _payload[0]&=0xffffffff00000000;
      _payload[0]|=t;
    }
   
    void setFileNumber(uint32_t f) {
      _payload[0]&=0x00000000ffffffff;
      _payload[0]|=uint64_t(f)<<32;
    }
   
    void setNumberOfEvents(uint32_t e) {
      _payload[1]&=0xffffffff00000000;
      _payload[1]|=e;
    }
   
    void setNumberOfBytes(uint32_t b) {
      _payload[1]&=0x00000000ffffffff;
      _payload[1]|=uint64_t(b)<<32;
    }
   
    void print(std::ostream &o=std::cout, std::string s="") const {
      o << s << "RecordContinuing::print()" << std::endl;
      RecordHeader::print(o,s+" ");
      
      for(unsigned i(0);i<payloadLength();i++) {
	o << s << "   Payload word " << std::setw(5) << " = 0x"
	  << std::hex << std::setfill('0')
	  << std::setw(16) << _payload[i]
	  << std::dec << std::setfill(' ') << std::endl;
      }
      
      o << s << "  Run number       = "
	<< std::setw(10) << runNumber() << std::endl;
      o << s << "  File number      = "
	<< std::setw(10) << fileNumber() << std::endl;
      o << s << "  Number of events = "
	<< std::setw(10) << numberOfEvents() << std::endl;
      o << s << "  Number of bytes  = "
	<< std::setw(10) << numberOfBytes() << std::endl;
    }
  private:
  };

}

#endif
