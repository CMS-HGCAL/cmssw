#ifndef hgcal_slinkfromraw_BePacketHeader_h
#define hgcal_slinkfromraw_BePacketHeader_h

#include <iostream>
#include <iomanip>
#include <cstdint>


namespace hgcal_slinkfromraw {

  class BePacketHeader {
  
  public:
    BePacketHeader() {
      reset();
    }
    
    void reset() {
      _data=0xbe00000fffffffff;
    }

    uint8_t pattern() const {
      return (_data>>56)&0xfe;
    }

    uint16_t bunchCounter() const {
      return (_data>>45)&0xfff;
    }
    
    uint8_t eventCounter() const {
      return (_data>>39)&0x3f;
    }

    uint8_t orbitCounter() const {
      return (_data>>36)&0x7;
    }

    uint8_t econdStatus(unsigned e) const {
      if(e>=12) return 0x7;
      return (_data>>(3*e))&0x7;
    }

    void setBunchCounter(uint16_t b) {
      _data&=0xfe001fffffffffff;
      _data|=uint64_t(b&0xfff)<<45;
    }
    
    void setEventCounter(uint8_t e) {
      _data&=0xffffe07fffffffff;
      _data|=uint64_t(e&0x3f)<<39;
    }
    
    void setOrbitCounter(uint16_t o) {
      _data&=0xffffff8fffffffff;
      _data|=uint64_t(o&0x7)<<36;
    }

    void setEcondStatus(unsigned e, uint8_t s) {
      if(e>=12) return;
      _data&=~(uint64_t(7)<<(3*e));
      _data|=uint64_t(s&0x7)<<(3*e);      
    }
    
    void print(std::ostream &o=std::cout, std::string s="") const {
      o << s << "BePacketHeader::print()" << std::endl;
      o << s << " Pattern = 0x"
        << std::hex << std::setfill('0')
        << std::setw(2) << unsigned(pattern())
        << std::dec << std::setfill(' ')
	<< std::endl;

      o << s << " Bunch counter = "
	<< std::setw(4) << bunchCounter()
	<< std::endl;
      o << s << " Event counter = "
	<< std::setw(4) << unsigned(eventCounter())
	<< std::endl;
      o << s << " Orbit counter = "
	<< std::setw(4) << unsigned(orbitCounter())
	<< std::endl;
      
      o << s << " ECON-D status bits = 0x"
	<< std::hex << std::setfill('0')
	<< std::setw(2) << unsigned(econdStatus(0));
      for(unsigned e(1);e<6;e++) {
	o << ", 0x" << std::setw(2) << unsigned(econdStatus(e));
      }
      o << std::endl;
      o << s << "                      0x"
	<< std::setw(2) << unsigned(econdStatus(6));
      for(unsigned e(7);e<12;e++) {
	o << ", 0x" << std::setw(2) << unsigned(econdStatus(e));
      }
      o << std::dec << std::setfill(' ') << std::endl;
    }
    
  private:
    uint64_t _data;
  };

}

#endif
