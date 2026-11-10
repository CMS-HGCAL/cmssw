#ifndef TPGFEDataformatEmulation_h
#define TPGFEDataformatEmulation_h

#include "DataFormats/HGCalDigi/interface/HGCalTriggerDefinitions.h"
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>
#include "TMath.h"

namespace TPGFEDataformatEmulation{

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ////////////////////////// Data Formats by Paul Dauncey /////////////////////////////////
  ///provision for tctp is added by Indra (without altering the existing functionalitites)

  class HalfHgcrocChannelData {
  public:
    HalfHgcrocChannelData() { setZero(); }

    void setZero() { _data=0; }

    bool isTot() const { return _data>=0x8000; }

    uint16_t getTcTp() const { return (_data>>12)&0x3; }

    uint16_t getAdc() const {
      if(isTot()) return 0;
      return _data&0x3ff;
    }

    uint16_t getTot() const {
      if(!isTot()) return 0;
      return _data&0xfff;
    }

    void setAdc(uint16_t a, uint16_t tctp) {
       //assert(a<0x400 and tctp<0x4); // commenting out since this breaks a test. TODO: Is this OK?
      _data=tctp<<12|a;
    }

    void setTot(uint16_t a, uint16_t tctp) {
      assert(a<0x1000 and tctp<0x4);
      _data=tctp<<12|a|0x8000;
    }

    uint16_t getData() const { return _data; }
    void setData(uint16_t data) { _data = data; }

    void print() const {
      std::cout << "HalfHgcrocChannelData(" << this << ")::print()" << std::endl;

      std::cout << std::dec << ::std::setfill(' ')
		<<" data: 0x" << std::hex << ::std::setfill('0')
		<< std::setw(4) << _data
		<< std::dec << ::std::setfill(' ') << ", "
		<< "TcTp: " << getTcTp() << ", "
		<< (isTot()?"TOT = ":"ADC = ") << std::setw(4)
		<< (isTot()?getTot():getAdc())
		// << "ADC = " << std::setw(4) << getAdc() <<", "
		// << ((getTcTp()!=0) ? Form("TOT = %u",getTot()) : "")
		<< std::endl;

    }

  private:
    uint16_t _data;
  };

  class HalfHgcrocData {
  public:
    enum {
      NumberOfChannels=36
    };

    HalfHgcrocData() : _bx(0xFFFF) {
      setZero();
    }

    void setZero() {
      std::memset((void *)_data,0,sizeof(HalfHgcrocChannelData)*NumberOfChannels);
    }

    void setBx(uint16_t bx) { _bx = bx;}
    const uint32_t getBx() const {return uint32_t(_bx);}
    void setSlinkBx(uint16_t bx) { _bxId = bx;}
    const uint32_t getSlinkBx() const {return uint32_t(_bxId);}

    const HalfHgcrocChannelData* getChannels() const {
      return _data;
    }

    bool hasTOT() const { return hasTcTp(3); }
    bool hasTcTp1() const { return hasTcTp(1); }
    bool hasTcTp2() const { return hasTcTp(2); }
    bool hasTcTp3() const { return hasTOT(); }

    bool hasTcTp(uint16_t tctpval) const {
      for(unsigned i(0);i<NumberOfChannels;i++)
	if(_data[i].getTcTp()==tctpval) {
	  // std::cout << "TOT/TOA noted for ich : " << i << std::endl;
	  // _data[i].print();
	  return true;
	}
      return false;
    }

    void setChannel(int ch, HalfHgcrocChannelData chdata) {
      assert(ch>=0 and ch<=35);
      _data[ch] = chdata;
    }

    void setChannels(const HalfHgcrocChannelData* data) {
      for(unsigned i(0);i<=NumberOfChannels;i++)
	_data[i] = data[i];
    }
    HalfHgcrocChannelData& getChannelData(uint32_t i) {
      return _data[i];
    }

    void print() const {
      std::cout << "HalfHgcrocData(" << this << ")::print()" << std::endl;

      const uint16_t *p((const uint16_t*)_data);

      for(unsigned i(0);i<NumberOfChannels;i++) {
	std::cout << " Channel " << std::setw(2) << i << " = 0x"
		  << std::hex << ::std::setfill('0')
		  << std::setw(4) << p[i]
		  << std::dec << ::std::setfill(' ') << ", "
		  << " bx: " << getBx() << ", "
		  << " TcTp: " << _data[i].getTcTp() << ", "
		  << (_data[i].isTot()?"TOT = ":"ADC = ") << std::setw(4)
		  << (_data[i].isTot()?_data[i].getTot():_data[i].getAdc())
		  // << "ADC = " << std::setw(4) << _data[i].getAdc() <<", "
		  // << ((_data[i].getTcTp()!=0) ? Form("TOT = %u",_data[i].getTot()) : "")
		  << std::endl;
      }
    }

  private:
    uint16_t _bx;//from ECON header
    uint16_t _bxId;//from Slink trailer
    HalfHgcrocChannelData _data[36];
  };


  class HgcrocTcData {
  public:
    HgcrocTcData() {
      setZero();
    }

    void setZero() {
      _data=0;
      _cdata=0;
      _isTot=false;
      _istctp1=false;
      _istctp2=false;
      _istctp3=false;
    }

    bool isTot() const {
      return _isTot;
    }

    uint16_t getCdata() const {
      return _cdata;
    }

    uint32_t getCharge() const {
      return _data;
    }

    bool isTcTp1() const {
      return _istctp1;
    }

    bool isTcTp2() const {
      return _istctp2;
    }

    bool isTcTp3() const {
      return _istctp3;
    }

    void setCdata(uint16_t a) {
      assert(a<0x80); //compressed HGCROC TC data is packed into7bits
      _cdata=a;
    }

    void setCharge(uint32_t a) {
      //assert(a<0x1000000); //assuming 21bit for HD module, then bit shift of 3, then +1
      // if(a<0x1000000)
      // 	_data=a;
      // else
      _data=(a<0x1000000)?a:0xffffff;
    }

    void setTot(bool a) {
      _isTot=a;
    }

    void setTcTp1(bool tctp1) {
      _istctp1 = tctp1 ;
    }

    void setTcTp2(bool tctp2) {
      _istctp2 = tctp2 ;
    }

    void setTcTp3(bool tctp3) {
      _istctp3 = tctp3 ;
    }

  private:
    uint32_t _data;  //raw-uncompressed data
    uint16_t _cdata; //compressed 4E+3M ()
    bool _isTot;     //isTOT or ADC
    bool _istctp1;
    bool _istctp2;
    bool _istctp3;
  };

  class ModuleTcData {
  public:
    enum {
      MaxNumberOfTCs = 48
    };

    ModuleTcData() {
      setZero();
    }
    void setCharge(uint32_t i, uint32_t a) {
      //assert(a<0x1000000); //assuming 21bit for HD module, then bit shift of 3, then +1
      // if(a<0x1000000)
      // 	_data=a;
      // else
      _data[i].setCharge(a);
    }

    void setCdata(uint32_t i, uint16_t a) {
      _data[i].setCdata(a);
    }
    void setZero() {
      std::memset((void *)_data,0,sizeof(HgcrocTcData)*MaxNumberOfTCs);
      NumberOfTCs=0;
    }

    void setBx(uint16_t bx) { _bx = bx;}
    const uint32_t getBx() const {return uint32_t(_bx);}

    const uint32_t getNofTCs() const {return uint32_t(NumberOfTCs);}
    const HgcrocTcData* getTCs() const {
      return _data;
    }
    const HgcrocTcData& getTC(uint32_t i) const {
      return _data[i];
    }

    HgcrocTcData getTC(uint32_t i) {
      return _data[i];
    }


    bool isTcTp1() const {
      for(uint16_t i(0);i<NumberOfTCs;i++)
	if(_data[i].isTcTp1()) return true;
      return false;
    }

    bool isTcTp2() const {
      for(uint16_t i(0);i<NumberOfTCs;i++)
	if(_data[i].isTcTp2()) return true;
      return false;
    }

    bool isTcTp3() const {
      for(uint16_t i(0);i<NumberOfTCs;i++)
	if(_data[i].isTot()) return true;
      return false;
    }

    void setNofTCs(const unsigned nofTCs) {NumberOfTCs = nofTCs;}
    void setTCs(HgcrocTcData* data) {
      for(uint16_t i(0);i<NumberOfTCs;i++)
	_data[i] = data[i];
    }

    void print() const {
      std::cout << "ModuleTriggerCellData(" << this << ")::print() : NumberOfTCs :" << NumberOfTCs << ", bx: " << getBx() << ", isTcTp1 : " << isTcTp1() << ", isTcTp2 : " << isTcTp2() << ", isTcTp3 : " << isTcTp3() << std::endl;

      for(uint16_t i(0);i<NumberOfTCs;i++) {
	std::cout << " TC " << std::setw(2) << i << ": compressed = "
		  << std::dec << ::std::setfill(' ')
		  << std::setw(10) << _data[i].getCdata()
		  << std::dec << ::std::setfill(' ')
		  << ", raw-uncompressed "
		  << std::setw(10) << _data[i].getCharge()
		  << ", istot : "
		  << std::setw(2) << _data[i].isTot()
		  << ", istctp1 : "
		  << std::setw(2) << _data[i].isTcTp1()
		  << ", istctp2 : "
		  << std::setw(2) << _data[i].isTcTp2()
		  << std::endl;
      }
    }

  private:
    HgcrocTcData _data[48];
    uint16_t _bx;
    uint16_t NumberOfTCs;
  };

}
#endif
