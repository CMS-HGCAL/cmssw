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
      void setSiChMapFile(std::string mapfile) {SiMapfname = mapfile;}
      void setSciChMapFile(std::string mapfile) {SciMapfname = mapfile;}
      void loadMuxMapping();
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
      void readSiChMapping();
      void readSciChMapping();
    
      //set the ROC ECONT configs from cmssw data formats
      void setRocConfig(uint32_t hrocidx, HGCalECONTConfig econtCfg);
      void setEconTConfig(uint32_t idx, HGCalECONTConfig econtCfg);

      void setPedThZero(); //set the pedestal and thresholds to zero
      void setPedZero(); //set only the ped values to zero keep the other roc config values as loaded from config
      
      //getters
      const uint32_t getSiModNhroc(std::string typecode) { return  SiModNhroc[typecode];}
      const uint32_t getSciModNhroc(std::string typecode) { return  SciModNhroc[typecode];}
      const std::map<std::string,std::vector<uint32_t>>& getSiModTClist() { return  SiModTClst;}
      const std::map<std::string,std::vector<uint32_t>>& getSiModSTClist() { return  SiModSTClst;}
      const std::map<std::string,std::vector<uint32_t>>& getSiModSTC16list() { return  SiModSTC16lst;}
      const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& getSiTCToROCpin() {return  SiTCToROCpin;}
      const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& getSiSTCToTC() {return  SiSTCToTC;}
      const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& getSiSTC16ToTC() {return  SiSTC16ToTC;}
      const std::map<std::pair<std::string,std::tuple<uint32_t,uint32_t,uint32_t>>,uint32_t>& getSiSeqToROCpin() {return SiSeqToRocpin;}
      const std::map<std::pair<std::string,uint32_t>,uint32_t>& getSiRocpinToAbsSeq() {return SiRocpinToAbsSeq;}
      
      const std::map<std::string,std::vector<uint32_t>>& getSciModTClist() { return  SciModTClst;}
      const std::map<std::string,std::vector<uint32_t>>& getSciModSTClist() { return  SciModSTClst;}
      const std::map<std::string,std::vector<uint32_t>>& getSciModSTC16list() { return  SciModSTC16lst;}
      const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& getSciTCToROCpin() {return  SciTCToROCpin;}
      const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& getSciSTCToTC() {return  SciSTCToTC;}
      const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& getSciSTC16ToTC() {return  SciSTC16ToTC;}
      const std::map<std::pair<std::string,std::tuple<uint32_t,uint32_t,uint32_t>>,uint32_t>& getSciSeqToROCpin() {return SciSeqToRocpin;}
      const std::map<std::pair<std::string,uint32_t>,uint32_t>& getSciRocpinToAbsSeq() {return SciRocpinToAbsSeq;}
      
      const std::map<std::tuple<uint32_t,uint32_t,uint32_t>,std::string>& getModIdxToName() {return modIdxToName;}
      const std::map<uint32_t,uint32_t>& getMuxMapping() {return refMuxMap;}
      
      std::map<uint32_t,TPGFEConfiguration::ConfigHfROC>& getRocPara() { return hroccfg;}
      std::map<uint64_t,TPGFEConfiguration::ConfigCh>& getChPara() { return hrocchcfg;}
      std::map<uint32_t,TPGFEConfiguration::ConfigEconT>& getEconTPara() { return econTcfg;}

      void printCfgPedTh(uint32_t moduleId);

    private:

      //channel <--> pin mapping and related variables
      std::string SiMapfname ;
      std::map<std::string,std::vector<uint32_t>>  SiModTClst ; std::map<std::string,std::vector<uint32_t>>  SiModSTClst; std::map<std::string,std::vector<uint32_t>>  SiModSTC16lst;
      std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SiTCToROCpin; std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SiTCToIJ;
      //std::map<std::pair<std::string,uint32_t>,uint32_t>  SiROCpinToTC; std::map<std::pair<std::string,uint32_t>,uint32_t>  SiIJToTC;
      //std::map<std::pair<std::string,uint32_t>,uint32_t>  SiTCToSTC;
      std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SiSTCToTC; std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SiSTC16ToTC;
      std::map<std::string,uint32_t> SiModNhroc;
      std::map<std::pair<std::string,std::tuple<uint32_t,uint32_t,uint32_t>>,uint32_t>  SiSeqToRocpin;
      std::map<std::pair<std::string,uint32_t>,uint32_t>  SiRocpinToAbsSeq;
      
      std::string SciMapfname ;
      std::map<std::string,std::vector<uint32_t>>  SciModTClst ; std::map<std::string,std::vector<uint32_t>>  SciModSTClst; std::map<std::string,std::vector<uint32_t>>  SciModSTC16lst;
      std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SciTCToROCpin; std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SciTCToIJ;
      //std::map<std::pair<std::string,uint32_t>,uint32_t>  SciROCpinToTC; std::map<std::pair<std::string,uint32_t>,uint32_t>  SciIJToTC;
      //std::map<std::pair<std::string,uint32_t>,uint32_t>  SciTCToSTC;
      std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SciSTCToTC; std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>  SciSTC16ToTC;
      std::map<std::string,uint32_t> SciModNhroc;
      std::map<std::pair<std::string,std::tuple<uint32_t,uint32_t,uint32_t>>,uint32_t>  SciSeqToRocpin;
      std::map<std::pair<std::string,uint32_t>,uint32_t>  SciRocpinToAbsSeq;
      
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
      std::map<uint32_t,uint32_t> refMuxMap;

    
  };
  void Configuration::readSiChMapping(){
  
    std::string Typecode;
    uint32_t ROC, HalfROC, Seq;
    std::string ROCpin;
    uint32_t ROCCH;
    int SiCell, TrLink, TrCell, iu, iv;
    float trace;
    int t;
    //Typecode ROC HalfROC Seq ROCpin SiCell TrLink TrCell iu iv trace t
    std::ifstream inwafermap(SiMapfname);
    std::string s;
    
    int isLD = -1;

    uint32_t prevHalfROC = 999;
    std::string prevTypecode = "";
    while(getline(inwafermap,s)){
      //std::cout << s.size() << std::endl;
      if(s.find("ML")!=std::string::npos or s.find("MH")!=std::string::npos){
        //std::cout << s << std::endl;
        std::stringstream ss(s);
        ss >> Typecode >> ROC >> HalfROC >> Seq >> ROCpin >> SiCell  >> TrLink  >> TrCell  >> iu  >> iv  >> trace  >> t ;
        if(ROCpin.find("CALIB")==std::string::npos and TrLink!=-1 and TrCell!=-1){
          //std::cout << s ;//<< std::endl;
          ROCCH = stoi(ROCpin);
          isLD = (Typecode.find("MH-")==std::string::npos)?1:0;
          uint32_t absTC = (isLD==1) ? (ROC*16 + TrLink*4 + TrCell) : (ROC*8 + TrLink*2 + TrCell) ;
          uint32_t absSTC = uint32_t(TMath::FloorNint(absTC/4));
          uint32_t absSTC16 = uint32_t(TMath::FloorNint(absTC/16));
          uint32_t rocpin = ROC*72 + ROCCH;
          //uint32_t iUiV = pck.packij(uint32_t(iu),uint32_t(iv)) ;
          std::tuple<uint32_t,uint32_t,uint32_t> seqch = std::make_tuple( ROC, HalfROC, Seq);
          uint32_t absSeq = (2*ROC+HalfROC)*37 + Seq ;
          
          SiTCToROCpin[std::make_pair(Typecode,absTC)].push_back( rocpin );
          //SiTCToIJ[std::make_pair(Typecode,absTC)].push_back( iUiV );
          // SiROCpinToTC[std::make_pair(Typecode,rocpin)] = absTC;
          // SiIJToTC[std::make_pair(Typecode,iUiV)] = absTC;
          SiSeqToRocpin[std::make_pair(Typecode,seqch)] = rocpin;
          SiRocpinToAbsSeq[std::make_pair(Typecode,rocpin)] = absSeq;
          
          if(prevHalfROC!=HalfROC or prevTypecode.compare(Typecode)!=0){
            if(prevTypecode.compare(Typecode)!=0){
              SiModNhroc[Typecode] = 1;
            }else{
              SiModNhroc[Typecode]++;
            }
            prevHalfROC = HalfROC;
            prevTypecode = Typecode ; 
          }
          if (std::find(SiModTClst[Typecode].begin(), SiModTClst[Typecode].end(), absTC) == SiModTClst[Typecode].end()) {
            SiModTClst[Typecode].push_back(absTC);
          }
          if (std::find(SiModSTClst[Typecode].begin(), SiModSTClst[Typecode].end(), absSTC) == SiModSTClst[Typecode].end()) {
            SiModSTClst[Typecode].push_back(absSTC);
          }
          if (std::find(SiModSTC16lst[Typecode].begin(), SiModSTC16lst[Typecode].end(), absSTC16) == SiModSTC16lst[Typecode].end()) {
            SiModSTC16lst[Typecode].push_back(absSTC16);
          }
          // SiTCToSTC[std::make_pair(Typecode,absTC)] = absSTC;
          if (std::find(SiSTCToTC[std::make_pair(Typecode,absSTC)].begin(), SiSTCToTC[std::make_pair(Typecode,absSTC)].end(), absTC) == SiSTCToTC[std::make_pair(Typecode,absSTC)].end()) {
            SiSTCToTC[std::make_pair(Typecode,absSTC)].push_back( absTC );
          }
          if (std::find(SiSTC16ToTC[std::make_pair(Typecode,absSTC16)].begin(), SiSTC16ToTC[std::make_pair(Typecode,absSTC16)].end(), absTC) == SiSTC16ToTC[std::make_pair(Typecode,absSTC16)].end()) {
            SiSTC16ToTC[std::make_pair(Typecode,absSTC16)].push_back( absTC );
          }

	        //std::cout <<"\t"<< absSTC << "\t" << absTC  <<"\t"<< rocpin <<"\t"<< iu <<"\t"<< iv << std::endl;
	      }//skip unconnected or calibration cells
      }//check type 
    }//loop over files
  
    // Alias symmetric wafers:
    // ML-B does not exist in WaferCellMapTraces.txt, but it is symmetric to ML-T.
    // MH-T does not exist in WaferCellMapTraces.txt, but it is symmetric to MH-B.
    auto aliasWafers = [&](const std::string& from, const std::string& to) {
      if (SiModNhroc.count(from)) SiModNhroc[to] = SiModNhroc[from];
      if (SiModTClst.count(from)) SiModTClst[to] = SiModTClst[from];
      if (SiModSTClst.count(from)) SiModSTClst[to] = SiModSTClst[from];
      if (SiModSTC16lst.count(from)) SiModSTC16lst[to] = SiModSTC16lst[from];
      
      for (const auto& pair : SiTCToROCpin) {
        if (pair.first.first == from) {
          SiTCToROCpin[std::make_pair(to, pair.first.second)] = pair.second;
        }
      }
      for (const auto& pair : SiTCToIJ) {
        if (pair.first.first == from) {
          SiTCToIJ[std::make_pair(to, pair.first.second)] = pair.second;
        }
      }
      for (const auto& pair : SiSeqToRocpin) {
        if (pair.first.first == from) {
          SiSeqToRocpin[std::make_pair(to, pair.first.second)] = pair.second;
        }
      }
      for (const auto& pair : SiRocpinToAbsSeq) {
        if (pair.first.first == from) {
          SiRocpinToAbsSeq[std::make_pair(to, pair.first.second)] = pair.second;
        }
      }
      for (const auto& pair : SiSTCToTC) {
        if (pair.first.first == from) {
          SiSTCToTC[std::make_pair(to, pair.first.second)] = pair.second;
        }
      }
      for (const auto& pair : SiSTC16ToTC) {
        if (pair.first.first == from) {
          SiSTC16ToTC[std::make_pair(to, pair.first.second)] = pair.second;
        }
      }
    };
    aliasWafers("ML-T", "ML-B");
    aliasWafers("MH-B", "MH-T");
  }

  void Configuration::readSciChMapping(){
    std::cout << "Yet to be implemented " << std::endl;
   }


  //void Configuration::readRocConfigYaml(const std::string& modName)
  void Configuration::setRocConfig( uint32_t hrocidx, HGCalECONTConfig econtCfg)
  {

    //const uint32_t nhfrocs = (pck.getDetType()==0)?getSiModNhroc(modName):getSciModNhroc(modName);
    const uint32_t nhfrocs = 6; // FIXME read it from econd cfg, dummy value at 6

    const uint32_t nrocs = TMath::CeilNint(nhfrocs/2);
    //const uint32_t nchs = 2*TPGFEDataformat::HalfHgcrocData::NumberOfChannels;

    // FIXME read from cfg, atm dummy values
    uint32_t th = 0; 
    uint64_t chmask = 0;
    uint32_t multfactor = 1;
    uint32_t dummy_tot_th = 0;
    uint32_t dummy_tot_p = 0;
    //uint32_t hrocid = hroccfg.size();

    //for(uint32_t ihroc=0;ihroc<nhfrocs;ihroc++){
      TPGFEConfiguration::ConfigHfROC hroc;
      hroc.setSelTC4(!econtCfg.density);
      hroc.setAdcTH(th);
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
    //}//roc loop
    //std::cout<<"============"<<std::endl;  
  }//end of read ped class

  void Configuration::loadMuxMapping(){
    //uint32_t type_F_muxmap[48] = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23, 22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28, 35, 34, 33, 32, 39, 38, 37, 36, 43, 42, 41, 40, 47, 46, 45, 44};
    uint32_t type_F_muxmap[48] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
    for(uint32_t itc=0;itc<48;itc++) refMuxMap[itc] = type_F_muxmap[itc];
      
  }

  void Configuration::setEconTConfig(uint32_t idx, HGCalECONTConfig econtCfg)
  {
    // FIXME fix config and read from
    uint32_t dummyCalib = 0x800; //this corresponds to the default value of unit gain
    //bool isFirst = true;
    //if(auto search_it = econTcfg.find(idx) ;  search_it != econTcfg.end()) isFirst = false;
    
    TPGFEConfiguration::ConfigEconT econt;
    //if (!isFirst) econt = econTcfg[idx];
    econt.setSelect(econtCfg.select);
    econt.setDensity(uint32_t(econtCfg.density));
    econt.setDropLSB(econtCfg.dropLSB);
    econt.setSTCType(econtCfg.stcType);
    econt.setNElinks(econtCfg.eportTxNumen);
    econt.setMSSumType(econtCfg.sumType);
    for(uint32_t itc=0;itc<48;itc++) {
      econt.setInputMux(itc,econtCfg.tcMux[itc]); // Use identity mapping as default
      econt.setCalibration(itc,dummyCalib);
    }

    econTcfg[idx] = econt;

   
  }

}

#endif
