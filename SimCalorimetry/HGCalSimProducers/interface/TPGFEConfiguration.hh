#ifndef TPGFEConfiguration_h
#define TPGFEConfiguration_h

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <map>


#include "TMath.h"
#include "TProfile.h"
#include "TSystem.h"
#include "TCanvas.h"
#include "TFile.h"

#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGFEDataformat.hh"
#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"


namespace TPGFEConfiguration{

  //The configuration of channel corresponding to ADC per module
  class ConfigCh {
  public:
    ConfigCh() {}
    uint32_t getAdcpedestal() const { return uint32_t(Adc_pedestal);}
    void setAdcpedestal(uint32_t ped) { Adc_pedestal = ped & 0xFF;}
    void print() {
      std::cout << std::dec << ::std::setfill(' ')
                << "ConfigCh(" << this << ")::print(): "
                <<"Adc_pedestal = "<< std::setw(4)<< getAdcpedestal()
                << std::endl;
    }
    void print(uint32_t ich) {
      std::cout << std::dec << ::std::setfill(' ')
                << "ConfigCh(" << this << ")::print(): "
                <<"ich: "<< ich <<", Adc_pedestal = "<< std::setw(4)<< getAdcpedestal()
                << std::endl;
    }

  private:
    uint8_t Adc_pedestal; //8-bits
  };


}
#endif
