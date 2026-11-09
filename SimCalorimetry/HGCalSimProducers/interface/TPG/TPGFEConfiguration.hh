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




  class Configuration{
    public:
      Configuration() {initId(); setTrainEWIndices(0,'w',0);}

      //setters
      void setRocPara(const std::map<uint32_t,HGCalROCTrigConfig>& cfghroc){
        for(auto const& hroc : cfghroc) hroccfg[hroc.first] = cfghroc.at(hroc.first);
      }
      void setChPara(const std::map<uint64_t,TPGFEConfiguration::ConfigCh>& cfghrocch) {
        for(auto const& hrocch : cfghrocch) hrocchcfg[hrocch.first] = cfghrocch.at(hrocch.first);
      }

      void setTrainEWIndices(uint32_t tr_index, char ew_c, uint32_t ew_index) { train_idx = tr_index ; ew = ew_c ; ew_idx = ew_index ;}
      //set the module related parameters
      void setModulePath(uint32_t zs, uint32_t sect, uint32_t lnk, uint32_t SiorSci, uint32_t econt_index, uint32_t LDorHD, uint32_t mod_index){
        zside = zs; sector = sect; link = lnk; det = SiorSci;
        econt = econt_index; selTC4 = LDorHD; module = mod_index;
      }
      void initId(){
        zside = 0, sector = 0, link = 0, det = 0;
        econt = 0, selTC4 = 1, module = 0, rocn = 0, half = 0;
      }

      //Read the channel <--> pin mapping from text file
      void setSiTCToChModule(const std::map<uint32_t,std::vector<uint32_t>>& TctoCh ) { SiTCToCh = TctoCh ;}
      // FIXME implement similar thing for tiles

      //set the ROC ECONT configs from cmssw data formats
      void setRocConfig(uint32_t hrocidx, const HGCalECONTConfig& econtCfg); // NOTE: Switched to "const HGCalECONTConfig&" instead of pass-by-value. - Jan
      void setEconTConfig(uint32_t idx, const HGCalECONTConfig& econtCfg); // NOTE: Switched to "const HGCalECONTConfig&" instead of pass-by-value. - Jan

      void setPedThZero(); //set the pedestal and thresholds to zero
      void setPedZero(); //set only the ped values to zero keep the other roc config values as loaded from config



      //getters
      std::map<uint32_t,std::vector<uint32_t>>& getSiTCToChModule(){return SiTCToCh;};
      // FIXME implement same for tiles

      const std::map<std::tuple<uint32_t,uint32_t,uint32_t>,std::string>& getModIdxToName() {return modIdxToName;}

      std::map<uint32_t,HGCalROCTrigConfig>& getRocPara() { return hroccfg;}
      std::map<uint64_t,TPGFEConfiguration::ConfigCh>& getChPara() { return hrocchcfg;}
      std::map<uint32_t,HGCalECONTConfig>& getEconTPara() { return econTcfg;}

      void printCfgPedTh(uint32_t moduleId);

    private:

      //channel <--> pin mapping and related variables
      std::map<uint32_t,std::vector<uint32_t>> SiTCToCh;


      std::map<std::tuple<uint32_t,uint32_t,uint32_t>,std::string>  modIdxToName; //detType, LD/HD, modindex

      //id definition

      uint32_t zside, sector, link, det;
      uint32_t econt, selTC4, module, rocn, half;

      //roc configuration
      std::string PedThfname;
      uint32_t train_idx;
      char ew;
      uint32_t ew_idx;
      std::map<uint32_t,HGCalROCTrigConfig> hroccfg; std::map<uint64_t,TPGFEConfiguration::ConfigCh> hrocchcfg;

      //econt configuration
      std::string EconTfname;
      std::map<uint32_t,HGCalECONTConfig> econTcfg;


  };

  void Configuration::setRocConfig( uint32_t hrocidx, const HGCalECONTConfig& econtCfg)
  {
    hroccfg[hrocidx] = econtCfg.hgcrocs[int(hrocidx/2)];
    // FIXME pedestals
    // for(uint32_t ich=0;ich<nchs;ich++){
    //   uint32_t ihalf = (ich<TPGFEDataformat::HalfHgcrocData::NumberOfChannels)?0:1;
    //   uint32_t chnl = ich%TPGFEDataformat::HalfHgcrocData::NumberOfChannels;
    //   uint32_t ped = 0; // FIXME dummy ped
    //   if(ihalf==0){
    //     TPGFEConfiguration::ConfigCh ch_0;
    //     ch_0.setAdcpedestal(ped);
    //     hrocchcfg[pck.packChId(rocid_0,chnl)] = ch_0;
    //   }else{
    //     TPGFEConfiguration::ConfigCh ch_1;
    //     ch_1.setAdcpedestal(ped);
    //     hrocchcfg[pck.packChId(rocid_1,chnl)] = ch_1;
    //   }
    // }//channel loop
   //std::cout<<"============"<<std::endl;
  }//end of read ped class

  void Configuration::setEconTConfig(uint32_t idx, const HGCalECONTConfig& cfg) { econTcfg[idx] = cfg; }

}

#endif
