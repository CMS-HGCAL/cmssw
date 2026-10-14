#ifndef Stage1IO_h
#define Stage1IO_h

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <random>

using namespace hgcal;
namespace TPGStage1 {

static void convertElinksToTcRawData(TPGFEDataformat::Type type, unsigned nTc,
              const uint32_t *v,
              TPGFEDataformat::TcRawDataPacket &vTcrdp) {
  
  std::vector<TPGFEDataformat::TcRawData> &vTc(vTcrdp.setTcData());
  


#ifdef EDM_ML_DEBUG 
  for(unsigned i(0);i<2;i++) {
    LogDebug("[HGCalUnpackerTrigger]") << "Elink " << i << " = 0x"
    << std::hex << std::setw(8) << std::setfill('0') << v[i]
    << std::dec << std::endl;
  }
#endif
  
  // ECONT header
  unsigned bx(v[0]>>ECONT_FRAME::HEADER_POS);

  LogDebug("[HGCalUnpackerTrigger]") << std::hex << " 32bit word: " << v[0]
            << std::dec << " ECONT header: " << bx << std::endl;
  
  bool bitMap(false);
  if(type==TPGFEDataformat::BestC) bitMap=(nTc>7);
  
  assert(nTc>0);
  vTc.resize(0);
  
  unsigned lastWord(0);
  unsigned lastBit(28);
  uint64_t d(v[0]);
  
  if(type==TPGFEDataformat::BestC) {
    lastBit-=8;
    vTcrdp.setTBM(type, bx, ((d>>lastBit)&0xff));
    if(false) vTcrdp.print();
  }else
    vTcrdp.setTBM(type, bx, 0);
  
  if(!bitMap) {
    for(unsigned tc(0);tc<nTc;tc++) {
if(lastBit<6) {
  d=(d<<32);
  lastWord++;
  //if(v.size()>lastWord) d|=v[lastWord];
  d|=v[lastWord];
  lastBit+=32;
}

if(type==TPGFEDataformat::BestC) {
  lastBit-=6;
  uint8_t tcAddr = ((d>>lastBit) & ECONT_FRAME::BC_LO_TCADDR_MASK);
  LogDebug("[HGCalUnpackerTrigger]")<< std::hex
        <<", d-word : 0x" << std::setfill('0') << std::setw(8) << (d>>lastBit)
        <<", masked-d-word : 0x" << std::setfill('0') << std::setw(8) << uint16_t(tcAddr)
        << std::dec << std::setfill(' ')
        <<std::endl;

  if ( tcAddr > ECONT_FRAME::BC_MAX_TCADDR) {
    throw cms::Exception("Stage1IORecoverable")
    << "convertElinksToTcRawData: TC address > " <<  ECONT_FRAME::BC_MAX_TCADDR << "\n"
    << "  tcAddr        = " <<  uint16_t(tcAddr) << "\n"
    << "  type       = " << type;
    ;
  } 
  vTc.push_back(TPGFEDataformat::TcRawData(type, tcAddr,0));
}
if(type==TPGFEDataformat::STC4A) {
  lastBit-=2;
  vTc.push_back(TPGFEDataformat::TcRawData(type,((d>>lastBit)&0x03),0));
}
if(type==TPGFEDataformat::STC4B) {
  lastBit-=2;
  vTc.push_back(TPGFEDataformat::TcRawData(type,((d>>lastBit)&0x03),0));
}
if(type==TPGFEDataformat::STC16) {
  lastBit-=4;
  vTc.push_back(TPGFEDataformat::TcRawData(type,((d>>lastBit)&0x0f),0));
}
    }

  } else {
    for(unsigned tc(0);tc<ECONT_FRAME::BC_MAX_TCADDR + 1;tc++) {
if(lastBit<1) {
  d=(d<<32);
  lastWord++;
  //if(v.size()>lastWord) d|=v[lastWord];
  d|=v[lastWord];
  lastBit+=32;
}

lastBit-=1;
if(((d>>lastBit)&0x1)!=0) {
  unsigned tcAdd = ECONT_FRAME::BC_MAX_TCADDR - tc; // bit map: LSB is the TC 0, MSB is TC 47
  LogDebug("[HGCalUnpackerTrigger]") << "index is of bit map is " << tc << " hence tc address is " << tcAdd << std::endl;
  vTc.push_back(TPGFEDataformat::TcRawData(type, tcAdd , 0)); 
#ifdef EDM_ML_DEBUG
  vTc.back().print();
#endif
}
    }
    LogDebug("[HGCalUnpackerTrigger]") << "vTc.size() = " << vTc.size() << ", nTc = " << nTc << std::endl;
          if (vTc.size() != nTc) {
    throw cms::Exception("Stage1IORecoverable")
    << "convertElinksToTcRawData: vTc.size() != nTc\n"
    << "  vTc.size() = " << vTc.size() << "\n"
    << "  nTc        = " << nTc << "\n"
    << "  type       = " << type;
    } 

    } 
  for(unsigned tc(0);tc<nTc;tc++) {
    if(lastBit<9) {
d=(d<<32);
lastWord++;
//if(v.size()>lastWord) d|=v[lastWord];
d|=v[lastWord];
lastBit+=32;
    }
    
    if(type==TPGFEDataformat::BestC) {
lastBit-=7;
unsigned idx = nTc - tc - 1; // to assign first energy to lowest tc address for high occupancy
if (bitMap) vTc[idx]=TPGFEDataformat::TcRawData(type,vTc[idx].address(),(d>>lastBit)&0x7f); 
else vTc[tc]=TPGFEDataformat::TcRawData(type,vTc[tc].address(),(d>>lastBit)&0x7f);
    } else if(type==TPGFEDataformat::STC4A) {
lastBit-=7;
vTc[tc]=TPGFEDataformat::TcRawData(type,vTc[tc].address(),(d>>lastBit)&0x7f);
    } else if(type==TPGFEDataformat::CTC4A) {
lastBit-=7;
vTc.push_back(TPGFEDataformat::TcRawData(type,tc,(d>>lastBit)&0x7f));
    } else if(type==TPGFEDataformat::CTC4B) {
lastBit-=9;
vTc.push_back(TPGFEDataformat::TcRawData(type,tc,(d>>lastBit)&0x1ff));
    } else {
lastBit-=9;
vTc[tc]=TPGFEDataformat::TcRawData(type,vTc[tc].address(),(d>>lastBit)&0x1ff);
    }
  }
  
  //if(bx==0xf && doPrint) {
#ifdef EDM_ML_DEBUG
  LogDebug("[HGCalUnpackerTrigger]") << "TcRawData words = " << vTc.size() << std::endl;
  for(unsigned i(0);i<vTc.size();i++) {
    vTc[i].print();
  }      
#endif

  
}
  
}
#endif
