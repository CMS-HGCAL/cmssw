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
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////The configuration of half of ROC based on HGCROC3a [doc. no. v2.0] (See Table@Page-43)
  //////EDMS ROCv3a: https://edms.cern.ch/ui/#!master/navigator/document?D:100570166:100570166:subDocs
  //////EDMS ROCv3b(recent): https://edms.cern.ch/ui/#!master/navigator/document?D:101362066:101362066:subDocs
  class ConfigHfROC {    
  public:
    ConfigHfROC() {}
    uint8_t getSelTC4() const {return selTC4 ;}
    uint32_t getAdcTH() const { return uint32_t(Adc_TH);}
    uint64_t getClrAdcTottrig() const { return ClrAdcTot_trig;}
    bool isChMasked(uint32_t ich) const {
      int chnl = ich%36;
      return (getClrAdcTottrig()>>chnl) & 0x1 ;
    }
    uint32_t getTotTH(uint32_t ich) const {
      uint32_t chnl = ich%36;
      uint32_t  tot_idx = TMath::FloorNint(chnl/9);
      return uint32_t(Tot_TH[tot_idx]);
    }
    uint32_t getTotP(uint32_t ich) const {
      uint32_t chnl = ich%36;
      uint32_t tot_idx = TMath::FloorNint(chnl/9);
      return uint32_t(Tot_P[tot_idx]);
    }
    uint32_t getMultFactor() const { return uint32_t(MultFactor);}    
    void setSelTC4(uint8_t seltc4) { selTC4 = seltc4;}
    void setAdcTH(uint32_t  adcth) { Adc_TH = adcth & 0x1F;}
    void setClrAdcTottrig(uint64_t clradctottrig) { ClrAdcTot_trig = clradctottrig & 0xFFFFFFFF;}
    void setMultFactor(uint32_t multfactor) { MultFactor = multfactor & 0x1F;}
    void setTotTH(uint32_t tot_idx, uint32_t tot_th) { Tot_TH[tot_idx] = tot_th & 0xFF;}
    void setTotP(uint32_t tot_idx, uint32_t tot_p) { Tot_P[tot_idx] = tot_p & 0x7F;}
    void print() const {
      std::cout << std::dec << ::std::setfill(' ')
                << "ConfigHfROC(" << this << ")::print(): "
                <<"Adc_TH = "<< std::setw(4) << getAdcTH()
                <<", MultFactor = "<< std::setw(3) << getMultFactor()
                << std::endl;
      
      std::cout << std::dec << ::std::setfill(' ')
                << "ConfigHfROC(" << this << ")::print(): "
                <<"ClrAdcTot_trig = ";
      for(uint32_t ich=0;ich<36;ich++) std::cout << std::setw(2) << "("<< ich <<": " << isChMasked(ich) <<") ";
      std::cout << std::endl;
      
      std::cout << std::dec << ::std::setfill(' ')
                << "ConfigHfROC(" << this << ")::print(): "
                <<"Tot_P = ";
      for(uint32_t itotch=0;itotch<4;itotch++) std::cout << std::setw(4) << "("<< itotch <<": " << uint32_t(Tot_P[itotch]) <<") ";
      std::cout << std::endl;
      
      std::cout << std::dec << ::std::setfill(' ')
                << "ConfigHfROC(" << this << ")::print(): "
                <<"Tot_TH = ";
      for(uint32_t itotch=0;itotch<4;itotch++) std::cout << std::setw(4) << "("<< itotch <<": " << uint32_t(Tot_TH[itotch]) <<") ";
      std::cout << std::endl;
    }

  private:    
    //Digital Info
    uint8_t selTC4;
    uint8_t Adc_TH; //5-bits
    uint64_t ClrAdcTot_trig;  //36-bits
    uint8_t MultFactor; //5-bits
    uint8_t Tot_P[4];  //one per 9 channel (each with 7-bits):not present in 2023 beam test
    uint8_t Tot_TH[4]; //one per 9 channel (each with 8 bits):not present in 2023 beam test    
  };

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
      void setRocPara(const std::map<uint32_t,TPGFEConfiguration::ConfigHfROC>& cfghroc){
        for(auto const& hroc : cfghroc) hroccfg[hroc.first] = cfghroc.at(hroc.first);
      }
      void setChPara(const std::map<uint64_t,TPGFEConfiguration::ConfigCh>& cfghrocch) {
        for(auto const& hrocch : cfghrocch) hrocchcfg[hrocch.first] = cfghrocch.at(hrocch.first);
      }

      void setEconTPara(const std::map<uint32_t,TPGFEConfiguration::ConfigEconT>& cfgeconT) {
      for(auto const& econT : cfgeconT) econTcfg[econT.first] = cfgeconT.at(econT.first);
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
      void setSiTCToChModule(std::map<uint32_t,std::vector<uint32_t>> TctoCh ) { SiTCToCh = TctoCh ;}
      // FIXME implement similar thing for tiles
    
      //set the ROC ECONT configs from cmssw data formats
      void setRocConfig(uint32_t hrocidx, HGCalECONTConfig econtCfg);
      void setEconTConfig(uint32_t idx, HGCalECONTConfig econtCfg);

      void setPedThZero(); //set the pedestal and thresholds to zero
      void setPedZero(); //set only the ped values to zero keep the other roc config values as loaded from config



      //getters
      std::map<uint32_t,std::vector<uint32_t>>& getSiTCToChModule(){return SiTCToCh;};
      // FIXME implement same for tiles
      
      const std::map<std::tuple<uint32_t,uint32_t,uint32_t>,std::string>& getModIdxToName() {return modIdxToName;}
      
      std::map<uint32_t,TPGFEConfiguration::ConfigHfROC>& getRocPara() { return hroccfg;}
      std::map<uint64_t,TPGFEConfiguration::ConfigCh>& getChPara() { return hrocchcfg;}
      std::map<uint32_t,TPGFEConfiguration::ConfigEconT>& getEconTPara() { return econTcfg;}

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
      std::map<uint32_t,TPGFEConfiguration::ConfigHfROC> hroccfg; std::map<uint64_t,TPGFEConfiguration::ConfigCh> hrocchcfg;

      //econt configuration
      std::string EconTfname;
      std::map<uint32_t,TPGFEConfiguration::ConfigEconT> econTcfg;

    
  };

  void Configuration::setRocConfig( uint32_t hrocidx, HGCalECONTConfig econtCfg)
  {
    // FIXME check if it makes more sense to have half roc or entire roc
    HGCalROCTrigConfig hgcrocConfig = econtCfg.hgcrocs[int(hrocidx/2)]; // is defined per ROC 
    
    // FIXME read from cfg, atm dummy values
    uint64_t chmask = 0;
    uint32_t multfactor = 1;
    uint32_t dummy_tot_th = 0;
    uint32_t dummy_tot_p = 0;

    TPGFEConfiguration::ConfigHfROC hroc;
    hroc.setSelTC4(!econtCfg.density);
    hroc.setAdcTH(hgcrocConfig.adc_th);
    hroc.setClrAdcTottrig(chmask);
    hroc.setMultFactor(multfactor);
    for(int itot=0;itot<4;itot++){
      hroc.setTotTH(itot, dummy_tot_th);
      hroc.setTotP(itot, dummy_tot_p);
    }

    hroccfg[hrocidx] = hroc;

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


  void Configuration::setEconTConfig(uint32_t idx, HGCalECONTConfig econtCfg)
  {

  
    TPGFEConfiguration::ConfigEconT econt;
    econt.setSelect(econtCfg.select);
    econt.setDensity(uint32_t(econtCfg.density));
    econt.setDropLSB(econtCfg.dropLSB);
    econt.setSTCType(econtCfg.stcType);
    econt.setNElinks(econtCfg.eportTxNumen);
    econt.setMSSumType(econtCfg.sumType);
    for (size_t itc = 0; itc < econtCfg.calv.size(); ++itc) {
      econt.setCalibration(itc,econtCfg.calv[itc]);
    }


    econTcfg[idx] = econt;

   
  }

}

#endif
