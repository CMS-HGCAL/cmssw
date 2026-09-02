#ifndef TPGFEModuleEmulation_h
#define TPGFEModuleEmulation_h

#include <iostream>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstddef>

#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGFEDataformat.hh"
#include "SimCalorimetry/HGCalSimProducers/interface/TPG/TPGFEConfiguration.hh"

namespace TPGFEModuleEmulation{


  //The following emulation work per event per module
  class HGCROCTPGEmulation{
  public:
    HGCROCTPGEmulation( TPGFEConfiguration::Configuration& cfgs) : configs(cfgs) {}
    //The following emulation function performs 1) pedestal subtraction, 2) linearization, 3) compression
    void Emulate(bool isSim,  const std::string& typecode, uint32_t& moduleId, std::map<uint32_t,TPGFEDataformat::HalfHgcrocData>&, TPGFEDataformat::ModuleTcData&);
    
    uint16_t CompressHgroc(uint32_t val, bool isldm){ // isldm stand for "is low density mode". It is determined by the SelTC4 parameter of HGCROC.
      
      //////The configuration of half of ROC based on HGCROC3a [doc. no. v2.0] (Read subsection 1.3.3 of Page-39)
      //////EDMS ROCv3a: https://edms.cern.ch/ui/#!master/navigator/document?D:100570166:100570166:subDocs
      //4E+3M
      //Ref:https://graphics.stanford.edu/%7Eseander/bithacks.html

      //controlled by the SelTC4 parameter of HGCROC (1/0 for LD/HD)
      
      val = (isldm)?val>>1:val>>3;
            
      uint32_t r = 0; // r will be log_2(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if (val > 7) {
        uint32_t v = val;
        r = 0;
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if (sub <= 0xF) {
          mant = (val >> shift) & 0x7;
        } else {
          sub = 0xF;
          mant = 0x7;
        }
      } else {
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
  
      uint16_t cdata = (sub<<3) | mant;
  
      return cdata;
    }
    
  private:    
    TPGFEConfiguration::Configuration& configs;
  };
  
  void HGCROCTPGEmulation::Emulate(bool isSim, const std::string& typecode,  uint32_t& moduleId, std::map<uint32_t,TPGFEDataformat::HalfHgcrocData>& rocdata, TPGFEDataformat::ModuleTcData& mdata){
  
    const std::map<uint32_t,std::vector<uint32_t>>& tcPinMap = configs.getSiTCToChModule(); // FIXME read the type of module

    uint16_t bx = 0xFFFF;
    const uint32_t nTCs = tcPinMap.size(); //read it from cfg econt
    bool selTC4 = 0;
    mdata.setNofTCs(nTCs);
    //for (const auto& [itc, pinlist] : tcPinMap) {
    for (auto it = tcPinMap.begin(); it != tcPinMap.end(); ++it)	    
    { // loop over TCs of the module
      auto itc = it->first;
      LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::HGCROCTPGEmulation::Emulate: tclist["<<itc<<"] " << std::endl;
      uint32_t totadc = 0; // accumulated charge of the TC, which will be compressed and stored in the output TcData. For simulation, it is either accumulated ADC or TOT depending on the signal type; for beam-test data, it is the accumulated ADC after pedestal subtraction and threshold cut, or the accumulated TOT after threshold cut and pedestal correction, depending on the signal type.
      bool isTot = false; // stand for whether the TC is triggered by TOT signal
      bool isTcTp1 = false; // stand for whether the TC is triggered by a channel with TC/Tp flag = 1
      bool isTcTp2 = false; // TC means trigger cell, and Tp means pass through. The TC/Tp flag information is provided in the input data for each channel, and is used to keep track of the signal type of the channels corresponding to the TC, which will be stored in the output TcData.
      bool isTcTp3 = false; // ...

      std::vector<uint32_t> pinlist = it->second;
      for(const auto& tcch : pinlist) 
      { // loop over sensor channels corresponding to the TC
       
        uint32_t rocpin = tcch%36 ; // the pin number of the channel within the ROC, which is needed for retrieving the channel data and parameters of the corresponding half-ROC.
        uint32_t rocn = TMath::Floor(tcch/72); // the ROC number
        uint32_t half = (int(TMath::Floor(tcch/36))%2==0)?0:1; // the half-ROC number
        uint32_t rocid = 2*rocn + half;

        if(rocdata.find(rocid)==rocdata.end()){ // if the half-ROC data is not found in the input data, skip the channel and print warning
          LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::HGCROCTPGEmulation::Emulate: HalfRoc not found in data for Tcch: "<< tcch << ", rocn: " << rocn << ", half: " << half << ", rocpin: " << rocpin << std::endl;
           continue ; 
        }

        LogDebug("[HGCalTriggerEmulator]")<<"\t TPGFEModuleEmulation::HGCROCTPGEmulation::Emulate: TC : " << itc << ", tcch: " << tcch <<", rocpin : "<<rocpin<<", rocid: "<<rocid<<", rocn: "<<rocn<<", half: "<<half<<std::endl; //print the channel data for the reference event

        TPGFEDataformat::HalfHgcrocChannelData& chdata = rocdata.at(rocid).getChannelData(rocpin); //get the channel data corresponding to the channel, which contains ADC, TOT, TC/Tp flag information. 
        bx = rocdata.at(rocid).getBx(); // get the bx information from the half-ROC data
        if(chdata.getTcTp()==1) isTcTp1 = true;
        if(chdata.getTcTp()==2) isTcTp2 = true;
        if(chdata.getTcTp()==3) isTcTp3 = true;

        // chdata.print(); 
        const TPGFEConfiguration::ConfigHfROC& rocpara = configs.getRocPara().at(rocid); //get the ROC parameters corresponding to the channel, which contains pedestal, threshold, TOT parameters, masking information, etc. needed for emulation
        selTC4 = rocpara.getSelTC4();
        if(!isSim){ //if not simulation, assumed beamtest data
          //uint32_t ped = configs.getChPara().at(pck.packChId(rocid,rocpin)).getAdcpedestal(); //get the pedestal of the channel, which is needed for pedestal subtraction in emulation
          //rocpara.print(); //print the ROC parameters for the reference event
          uint32_t ped = 0; //FIXME

          if(!chdata.isTot()){ // if the signal is ADC, perform pedestal subtraction and threshold cut, and accumulate the ADC if the channel is not masked and pedestal is valid. If the channel is masked or pedestal is invalid, set the ADC to 0.
            unsigned thr = rocpara.getAdcTH(); 
            uint32_t adc = chdata.getAdc();
            // if(chdata.getTcTp()==1)
            //   if(itc!=20 or rocid!=770) adc = 0;
            adc = (adc>(ped+thr) and !(rocpara.isChMasked(rocpin)) and (ped<0xFF)) ? adc-ped : 0 ;
            totadc += adc; // accumulate the ADC of the channels corresponding to the TC
            LogDebug("[HGCalTriggerEmulator]")<<"\t TPGFEModuleEmulation::HGCROCTPGEmulation::Emulate: ped: " << ped << ", thr: " << thr <<", adc : "<<adc<<", rocpara.isChMasked(rocpin): "<< rocpara.isChMasked(rocpin) <<std::endl; //print the pedestal, threshold, ADC after pedestal subtraction and threshold cut, and masking information for the reference event
          }
          else{ // if the signal is TOT, perform threshold cut and pedestal correction.
            uint32_t tot1 = (chdata.getTot()>=rocpara.getTotTH(rocpin)) ? (chdata.getTot()-rocpara.getTotP(rocpin)) : (rocpara.getTotTH(rocpin)-rocpara.getTotP(rocpin)) ;
            //ideally ped<255 condition should only be restricted for ADC case, however TOT signal triggers in emulation for event 146245 of relay 1695829026 and link 1, which is not that seen by ECONT data
            uint32_t totlin = (!(rocpara.isChMasked(rocpin)) and (ped<0xFF))?tot1*rocpara.getMultFactor():0;  // perform pedestal correction and linearization for TOT signal, and set the TOT to 0 if the channel is masked or pedestal is invalid.
            totadc += totlin;
            //isTot = true;
            LogDebug("[HGCalTriggerEmulator]")<<"\t TPGFEModuleEmulation::HGCROCTPGEmulation::Emulate: tot1: " << tot1 << ", tot: " << chdata.getTot() <<", thr : "<< rocpara.getTotTH(rocpin) <<", ped: "<< rocpara.getTotP(rocpin) << ", totlin : "<< totlin <<std::endl;
          }//istot or adc
        }
        else{ // if simulation, directly accumulate ADC or TOT depending on the signal type without pedestal subtraction, threshold cut, and linearization, as the input simulation data is assumed to be already prepared with ideal detector response and perfectly calibrated.
          //check CMSSW for details should be 
          if(!chdata.isTot())
            totadc += chdata.getAdc(); 
          else
            totadc += chdata.getTot(); 
        } //isCMSSW simulation of beam-test analysis
      } //loop of sensor channels
      
      mdata.setCharge(itc, totadc);
      mdata.getTC(itc).setTot(isTot); //stand for tctp==3
      mdata.getTC(itc).setTcTp1(isTcTp1);
      mdata.getTC(itc).setTcTp2(isTcTp2);
      mdata.getTC(itc).setTcTp3(isTcTp3);
      mdata.setCdata(itc, CompressHgroc(totadc,selTC4 ));
  
    }//loop over TCs
    mdata.setBx(bx);
    //std::cout << "\t TPGFEModuleEmulation::HGCROCTPGEmulation::Emulate: " << std::endl;
    //mdata.print();

  }

  

  class ECONTEmulation{
  public:
    ECONTEmulation(TPGFEConfiguration::Configuration& cfgs) : configs(cfgs) {}

    ~ECONTEmulation() {}
    
    //The following emulation function performs 1) decompression, 2) calibration, 3) compression
    void Emulate(bool isSim, const std::string& typecode, uint32_t& moduleId, const TPGFEDataformat::ModuleTcData&, TPGFEDataformat::TcRawDataPacket& );



    void EmulateSTC(
      bool isSim,  uint32_t& moduleId, const TPGFEDataformat::ModuleTcData&
    );
    void EmulateBC(
      bool isSim, const std::string& typecode,  uint32_t& moduleId, const TPGFEDataformat::ModuleTcData&, TPGFEDataformat::TcRawDataPacket&
    );
    
    const TPGFEDataformat::TcModulePacket& getTcRawDataPacket() const {return emulOut;}
    TPGFEDataformat::TcModulePacket& accessTcRawDataPacket() {return emulOut;}
    void setVerbose(bool verbose = true) {isVerbose = verbose;}
    
    // BX here may be absorbed into TcRawDataPacket?
    static void convertToElinkData(unsigned bx, const TPGFEDataformat::TcRawDataPacket &tcrdp, uint32_t *ve);
    void getElinkData(unsigned bx, uint32_t *ptr) const;

    void fillRandomTcRawData(unsigned bx);
    void fillZeroEnergyTcRawData(unsigned bx);
    
    static void generateTcRawData(bool zero, unsigned bx,
				  TPGFEDataformat::Type type,
				  unsigned nTc,
				  TPGFEDataformat::TcRawDataPacket &vtcrp);
    static void generateRandomTcRawData(unsigned bx,
					TPGFEDataformat::Type type,
					unsigned nTc,
					TPGFEDataformat::TcRawDataPacket &vtcrp);
    static void generateZeroEnergyTcRawData(unsigned bx,
					    TPGFEDataformat::Type type,
					    unsigned nTc,
					    TPGFEDataformat::TcRawDataPacket &vtcrp);
    bool disableTcSafety = false;
    
  private:
    uint32_t DecompressEcont(uint16_t compressed, bool density){
      //4E+3M with midpoint correction
      //controlled by the density parameter of ECON-T (0/1 stands for LD/HD and represents 1/3 bit shifts)
      uint32_t mant = compressed & 0x7;
      uint32_t expo = (compressed>>3) & 0xF;
        
      if(expo==0) return (density) ? (mant<<3)+4 : (mant<<1)+1 ; //The the +4/+1 are midpoint corrections for expo==0
        
      uint32_t shift = expo+2; 
                shift += (density) ? 3 : 1;
      uint32_t decomp = 1<<shift; 
      uint32_t mpdeco = 1<<(shift-4);
      decomp = decomp | (mant<<(shift-3));
      decomp = decomp | mpdeco;
       return decomp;
    }

    uint16_t CompressEcontStc4E3M(uint64_t val, uint32_t dropLSB){
      //4E+3M
      //controlled by the dropLSB parameter of ECON-T 
      val = val>>dropLSB;  
      
      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if(val>7){
        uint64_t v = val; 
        r = 0; 
	      while (v >>= 1) r++;
	      sub = r - 2;
	      shift = r - 3;
        if(sub<=0xF){
          mant = (val>>shift) & 0x7;
        }else{
          sub = 0xF;
          mant = 0x7;
        }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
      sub = sub & 0xF;
      mant = mant & 0x7;
      
      uint16_t packed = (sub<<3) | mant;
      
      return packed;
    }
    
    uint16_t CompressEcontStc5E4M(uint64_t val){
      //5E+4M
      //dropLSB is not applicable for 5E+4M

      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if(val>0xF){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 3;         
        shift = r - 4;       
        if(sub<=0x1F){
          mant = (val>>shift) & 0xF;
        }else{
          sub = 0x1F;
          mant = 0xF;
        }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0xF;
      }

      sub = sub & 0x1F;
      mant = mant & 0xF;

      uint16_t packed = (sub<<4) | mant;
      
      return packed;
    }
    
    uint16_t CompressEcontBc(uint64_t val, uint32_t dropLSB){
      //4E+3M
      //controlled by the dropLSB parameter of ECON-T 
      val = val>>dropLSB;  
      
      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
  
      if(val>7){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if(sub<=0xF){
          mant = (val>>shift) & 0x7;
        }else{
          sub = 0xF;
          mant = 0x7;
        }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
      sub = sub & 0xF;
      mant = mant & 0x7;

      uint16_t packed = (sub<<3) | mant;
      
      return packed;
    }
    
    uint16_t CompressEcontModsum(uint64_t val, uint32_t dropLSB){
      //5E+3M
      //controlled by the dropLSB parameter of ECON-T 
      //val = val>>dropLSB;  

      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if(val>7){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if(sub<=0x1F){
          mant = (val>>shift) & 0x7;
        }else{
          sub = 0x1F;
          mant = 0x7;
        }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
      
      sub = sub & 0x1F;
      mant = mant & 0x7;
      
      uint16_t packed = (sub<<3) | mant;
 
      return packed;
    }
    

    void batcherOEMSort(std::vector<TPGFEDataformat::TcRawData>& tc) {
      //C++ adaptation of batcher odd-even sorting of https://github.com/dnoonan08/ECONT_Emulator/ASICBlocks/bestchoice.py
      //Requires 49 Tcs for sorting
      TPGFEDataformat::TcRawData dummy(TPGFEDataformat::Unknown,0x3f, 0, 0, false, false, false);
      tc.push_back(dummy);
      
      uint32_t N = uint32_t(tc.size());
      uint32_t t = uint32_t(ceil(log(N)/log(2)));
      uint32_t p = uint32_t(pow(2,(t-1)));
      while(p>=1){
        uint32_t q = uint32_t(pow(2,(t-1)));
        uint32_t r = 0;
        uint32_t d = p;
	      while (q>=p){
          for(uint32_t i=0; i<(N-d) ; i++){
            if ((i & p) != r) continue;
            if (tc[i] < tc[i+d]) std::swap(tc[i], tc[i+d]);
          }
          d = q - p;
          q = floor(q/2);
          r = p;
	      }
	      p = floor(p/2);
      }
      tc.pop_back(); 

    }

    uint32_t findLastMax(std::vector<TPGFEDataformat::TcRawData>& tc) {
      for(uint32_t itc = 0 ; itc< uint32_t(tc.size()-1); itc++) if(tc[itc+1] < tc[0]) return itc;
      return uint32_t(tc.size()-1);
    }

    bool isTie(std::vector<TPGFEDataformat::TcRawData>& tc) {
      for(uint32_t itc = 0 ; itc< tc.size(); itc++) if(tc[itc].energy() !=  tc[0].energy()) return false;
      return true;
    }

    bool isAllZero(std::vector<TPGFEDataformat::TcRawData>& tc) {
      for(uint32_t itc = 0 ; itc< tc.size(); itc++) if(tc[itc].energy()>0) return false;
      return true;
    }


    TPGFEConfiguration::Configuration& configs;
    TPGFEDataformat::TcModulePacket emulOut;
    bool isVerbose = true; 
  };

  void ECONTEmulation::Emulate(
    bool isSim,  const std::string& typecode,  uint32_t& moduleId, const TPGFEDataformat::ModuleTcData& mdata, TPGFEDataformat::TcRawDataPacket& rdp
  )
  {

    const TPGFEDataformat::Type& outputType = configs.getEconTPara().at(moduleId).getOutType();

    if(outputType==TPGFEDataformat::BestC) 
      EmulateBC(isSim, typecode,  moduleId, mdata, rdp);
    else if(outputType==TPGFEDataformat::STC4A or outputType==TPGFEDataformat::STC4B or outputType==TPGFEDataformat::STC16 or TPGFEDataformat::CTC4A or TPGFEDataformat::CTC4B)
      EmulateSTC(isSim,  moduleId, mdata); // FIXME to be implemented, follow BC
  }

  
  void ECONTEmulation::EmulateSTC(
    bool isSim,  uint32_t& moduleId, const TPGFEDataformat::ModuleTcData& mdata
  )
  {  
    // const std::map<std::tuple<uint32_t,uint32_t,uint32_t>,std::string>& modNameMap = configs.getModIdxToName();
    // const std::map<uint32_t,uint32_t>& refMuxMap = configs.getMuxMapping() ;
    // const std::string& typecode = "ML-F"; //FIXME
    // uint32_t dropLSB = configs.getEconTPara().at(moduleId).getDropLSB() ;
    // uint32_t nofSTCs = configs.getEconTPara().at(moduleId).getNofSTCs();
    // TPGFEDataformat::TcRawData dummy(TPGFEDataformat::Unknown,0x3f, 0, 0, false, false, false);
    
    // const TPGFEDataformat::Type& outputType = configs.getEconTPara().at(moduleId).getOutType();
   
    // if(outputType==TPGFEDataformat::STC4A or outputType==TPGFEDataformat::STC4B or outputType==TPGFEDataformat::CTC4A or outputType==TPGFEDataformat::CTC4B){
      
    //   const std::map<std::string,std::vector<uint32_t>>& modSTClist = (pck.getDetType()==0)?configs.getSiModSTClist():configs.getSciModSTClist();
    //   const std::vector<uint32_t>& stclist = modSTClist.at(typecode) ;
    //   const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& stcTcMap = (pck.getDetType()==0)?configs.getSiSTCToTC():configs.getSciSTCToTC();
    //   uint16_t bx = 0xffff;
    //   emulOut.second.reset();
    //   for(const auto& istc : stclist)
    //   { // loop over STCs of the module
    //     bool isTcTp1 = false;
    //     bool isTcTp2 = false;
    //     bool isTcTp3 = false;

    //     if(istc>=nofSTCs) continue;

    //     const std::vector<uint32_t>& tclist = stcTcMap.at(std::make_pair(typecode,istc));
    //     bx = (mdata.getBx()==3564) ? 0xF : mdata.getBx() & 0x7 ; 
    //     uint64_t decompressedSTC = 0;
    //     std::vector<TPGFEDataformat::TcRawData> tcrawdatalist;
    //     TPGFEDataformat::TcRawData tcdata;

    //     for(const auto& econtc : tclist)
    //     { // loop over TCs corresponding to the STC
    //       uint32_t hgctc = configs.getEconTPara().at(moduleId).getInputMux(econtc) ;
    //       bool hasFound = false;
    //       uint32_t emultc = 0xffffffff;
    //       for (const auto& it : refMuxMap)
    //       {
    //         if (it.second == hgctc)  
    //         {
    //           hasFound = true;
    //           emultc = it.first;
    //         }
    //       }
    //       if(!hasFound)
    //       {
    //         std::cerr << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC (moduleid="<<moduleId<<") : Mux not set for TC " << econtc << std::endl;
    //         continue;
    //       }

    //       if(mdata.getTC(emultc).isTcTp1()) isTcTp1 = true;
    //       if(mdata.getTC(emultc).isTcTp2()) isTcTp2 = true;
    //       if(mdata.getTC(emultc).isTcTp3()) isTcTp3 = true;

    //       uint64_t decompressed = DecompressEcont(mdata.getTC(emultc).getCdata(),configs.getEconTPara().at(moduleId).getDensity());

    //       uint64_t decomp64bit =  decompressed * configs.getEconTPara().at(moduleId).getCalibration(econtc) ;
          
    //       LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC4 (moduleid="<<moduleId<<", TC="<<econtc<<", hgctc="<<hgctc<<") decompressed: " << decompressed << ", decompressed*calib: " << decomp64bit << std::endl; 
          
    //       decompressed =  decomp64bit >> 11;
    //       decompressedSTC += decompressed  ;

    //       LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC4 (moduleid="<<moduleId<<", TC="<<econtc<<") decompressed*calib>>11: " << decompressed << ", decompressedSTC: " << decompressedSTC << std::endl; 

    //       uint16_t compressed_bc = CompressEcontBc(decompressed, dropLSB);
    //       tcdata.setTriggerCell(TPGFEDataformat::BestC, econtc, compressed_bc, decompressed, mdata.getTC(emultc).isTcTp1(), mdata.getTC(emultc).isTcTp2(), mdata.getTC(emultc).isTcTp3()) ;
    //       tcrawdatalist.push_back(tcdata);
    //     }
    //     LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC4 (moduleid="<<moduleId<<", STC="<< istc <<") tcrawdatalist.size: " << tcrawdatalist.size() << std::endl;

    //     std::sort(tcrawdatalist.begin(),tcrawdatalist.end(),TPGFEDataformat::TcRawDataPacket::customGTE);

    //     uint32_t lMaxId = isTie(tcrawdatalist) ? 3 : findLastMax(tcrawdatalist);
    //     tcrawdatalist.resize(lMaxId+1);

    //     LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC4 Before sorting....(moduleid="<<moduleId<<", STC="<< istc <<"), modified size: " << tcrawdatalist.size() << std::endl;

    //     if(isVerbose) for(int i=0;i<tcrawdatalist.size();i++) tcrawdatalist[i].print();

    //     std::sort(tcrawdatalist.begin(),tcrawdatalist.end(),TPGFEDataformat::TcRawDataPacket::customLTA);
    //     LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC4 After sorting....(moduleid="<<moduleId<<", STC="<< istc <<"), modified size: " << tcrawdatalist.size() << std::endl;
    //     if(isVerbose) for(int i=0;i<tcrawdatalist.size();i++) tcrawdatalist[i].print();
    //     TPGFEDataformat::TcRawData firstMinA = tcrawdatalist[0] ;
    //     if(isVerbose) firstMinA.print();
    //     uint16_t compressed_energy = (outputType==TPGFEDataformat::STC4A or outputType==TPGFEDataformat::CTC4A) ? CompressEcontStc4E3M(decompressedSTC, dropLSB) : CompressEcontStc5E4M(decompressedSTC);
    //     uint64_t raw_E = (outputType==TPGFEDataformat::STC4A or outputType==TPGFEDataformat::CTC4A) ? decompressedSTC>>dropLSB : decompressedSTC ;
    //     emulOut.second.setTBM(outputType, bx, 0); 
    //     if(outputType==TPGFEDataformat::STC4A or outputType==TPGFEDataformat::STC4B)
    //       emulOut.second.setTcData(outputType, firstMinA.address()%4, compressed_energy, raw_E, isTcTp1, isTcTp2, isTcTp3);
    //     else
    //       emulOut.second.setTcData(outputType, istc, compressed_energy, raw_E, isTcTp1, isTcTp2, isTcTp3);
    //     tcrawdatalist.clear();
    //   }//stc loop
      
    // }
    
    // if(outputType==TPGFEDataformat::STC16)
    // {  
    //   const std::map<std::string,std::vector<uint32_t>>& modSTC16list = (pck.getDetType()==0)?configs.getSiModSTC16list():configs.getSciModSTC16list();
    //   const std::vector<uint32_t>& stc16list = modSTC16list.at(typecode) ;
    //   const std::map<std::pair<std::string,uint32_t>,std::vector<uint32_t>>& stc16TcMap = (pck.getDetType()==0)?configs.getSiSTC16ToTC():configs.getSciSTC16ToTC();
    //   uint16_t bx = 0xffff;
    //   emulOut.second.reset();
    //   for(const auto& istc16 : stc16list)
    //   {
    //     bool isTcTp1 = false;
    //     bool isTcTp2 = false;
    //     bool isTcTp3 = false;
    //     if(istc16>=nofSTCs) continue;
    //     const std::vector<uint32_t>& tclist = stc16TcMap.at(std::make_pair(typecode,istc16));
    //     bx = (mdata.getBx()==3564) ? 0xF : mdata.getBx() & 0x7 ; 
    //     uint64_t decompressedSTC16 = 0;
    //     std::vector<TPGFEDataformat::TcRawData> tcrawdatalist;
    //     TPGFEDataformat::TcRawData tcdata;
    //     for(const auto& econtc : tclist)
    //     {
    //       uint32_t hgctc = configs.getEconTPara().at(moduleId).getInputMux(econtc) ;
    //       bool hasFound = false;
    //       uint32_t emultc = 0xffffffff;
    //       for (const auto& it : refMuxMap)
    //       {
    //         if (it.second == hgctc)  
    //         {
    //           hasFound = true;
    //           emultc = it.first;
    //         }
    //       }
    //       if(!hasFound)
    //       {
    //         std::cerr << "TPGFEModuleEmulation::ECONTEmulation::EmulateBC (moduleid="<<moduleId<<") : Mux not set for TC " << econtc << std::endl;
    //         continue;
    //       }
    //       if(mdata.getTC(emultc).isTcTp1()) isTcTp1 = true;
    //       if(mdata.getTC(emultc).isTcTp2()) isTcTp2 = true;
    //       if(mdata.getTC(emultc).isTcTp3()) isTcTp3 = true;
    //       uint64_t decompressed = DecompressEcont(mdata.getTC(emultc).getCdata(), configs.getEconTPara().at(moduleId).getDensity());
    //       uint64_t decomp64bit = decompressed * configs.getEconTPara().at(moduleId).getCalibration(econtc) ;
    //       LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC16 (moduleid="<<moduleId<<", TC="<<econtc<<", hgctc="<<hgctc<<") decompressed: " << decompressed << ", decompressed*calib: " << decomp64bit << std::endl; 
    //       decompressed =  decomp64bit >> 11;
    //       decompressedSTC16 += decompressed ;
    //       LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateSTC16 (moduleid="<<moduleId<<", TC="<<econtc<<") decompressed*calib>>11: " << decompressed << ", decompressedSTC16: " << decompressedSTC16 << std::endl; 
    //       uint16_t compressed_bc = CompressEcontBc(decompressed,dropLSB);
    //       tcdata.setTriggerCell(TPGFEDataformat::BestC, econtc, compressed_bc, decompressed, mdata.getTC(emultc).isTcTp1(), mdata.getTC(emultc).isTcTp2(), mdata.getTC(emultc).isTcTp3()) ;
    //       tcrawdatalist.push_back(tcdata);
    //     }
    //     // batcherOEMSort(tcrawdatalist);
    //     // uint32_t lMaxId = findLastMax(tcrawdatalist);
    //     // tcrawdatalist.resize(lMaxId+1);
    //     std::sort(tcrawdatalist.begin(),tcrawdatalist.end(),TPGFEDataformat::TcRawDataPacket::customGTE);
    //     uint32_t lMaxId = isTie(tcrawdatalist) ? 3 : findLastMax(tcrawdatalist);
    //     tcrawdatalist.resize(lMaxId+1);
    //     std::sort(tcrawdatalist.begin(),tcrawdatalist.end(),TPGFEDataformat::TcRawDataPacket::customLTA);
    //     TPGFEDataformat::TcRawData firstMinA = tcrawdatalist[0] ;
    //     uint16_t compressed_energy = CompressEcontStc5E4M(decompressedSTC16);
    //     emulOut.second.setTBM(outputType, bx, 0); 
    //     emulOut.second.setTcData(outputType, (firstMinA.address())%16, compressed_energy, decompressedSTC16, isTcTp1, isTcTp2, isTcTp3);
    //     tcrawdatalist.clear();
    //   }//stc16 loop
    // }//STC16 select condition
    
  }//Emulate STC


  
  void ECONTEmulation::EmulateBC(
      bool isSim, const std::string& typecode, uint32_t& moduleId, const TPGFEDataformat::ModuleTcData& mdata, TPGFEDataformat::TcRawDataPacket& rdp
  ) 
  {
    isVerbose = true;
    uint32_t dropLSB = configs.getEconTPara().at(moduleId).getDropLSB() ;
 
    const TPGFEDataformat::Type& outputType = configs.getEconTPara().at(moduleId).getOutType();

    uint32_t nTCs =  mdata.getNofTCs();
    uint16_t bx = (mdata.getBx()==3564) ? 0xF : mdata.getBx() & 0x7 ; //make 8 modulo
    uint64_t decompressedMS = 0;
    std::vector<TPGFEDataformat::TcRawData> tcrawdatalist;
    TPGFEDataformat::TcRawData tcdata;

    for(uint32_t econtc = 0; econtc < nTCs;  econtc++)
    {

      uint64_t decompressed = DecompressEcont(mdata.getTC(econtc).getCdata(), configs.getEconTPara().at(moduleId).getDensity()); //funny that return needs to defined as 64-bit
      uint64_t decomp64bit = decompressed * configs.getEconTPara().at(moduleId).getCalibration(econtc); //overflows for 12bit TOT
      LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateBC (moduleid="<<moduleId<<", TC="<<econtc<<") decompressed: " << decompressed << ", decompressed*calib: " << decomp64bit << std::endl; 
      decompressed =  decomp64bit >> 11;
      decompressedMS += (decompressed >> dropLSB) ;
      LogDebug("[HGCalTriggerEmulator]") << "TPGFEModuleEmulation::ECONTEmulation::EmulateBC (moduleid="<<moduleId<<", TC="<<econtc<<") decompressed*calib>>11: " << decompressed << ", decompressedMS: " << decompressedMS << std::endl; 
      uint16_t compressed_bc = CompressEcontBc(decompressed,dropLSB);
      tcdata.setTriggerCell(outputType, econtc, compressed_bc, decompressed>>dropLSB, mdata.getTC(econtc).isTcTp1(), mdata.getTC(econtc).isTcTp2(), mdata.getTC(econtc).isTcTp3()) ;
      tcrawdatalist.push_back(tcdata);
    }
    batcherOEMSort(tcrawdatalist);
    //for(int itc=0;itc<int(tcrawdatalist.size());itc++) tcrawdatalist[itc].print();
    
    uint16_t compressed_modsum = CompressEcontModsum(decompressedMS,dropLSB);
    rdp.reset();
    rdp.setTBM(outputType, bx, compressed_modsum, decompressedMS>>dropLSB); 
    uint32_t realTcSize = tcrawdatalist.size();
    if (realTcSize > 0 && tcrawdatalist.back().address() == 0x3f) {
      realTcSize--;
    }
    uint32_t nofBCTcs = configs.getEconTPara().at(moduleId).getBCType();
    if (nofBCTcs > realTcSize) {
      if (!disableTcSafety) {
        std::cerr << "TPGFEModuleEmulation Warning: Only found " << realTcSize << " TCs for module " << moduleId << " but expected " << nofBCTcs << std::endl;
      }
      nofBCTcs = realTcSize;
    }
    for(uint32_t itc = 0 ; itc<nofBCTcs ; itc++)
      rdp.setTcData(outputType, tcrawdatalist[itc].address(), tcrawdatalist[itc].energy(), tcrawdatalist[itc].rawE(),tcrawdatalist[itc].isTcTp1(),tcrawdatalist[itc].isTcTp2(),tcrawdatalist[itc].isTcTp3());
  }//Emulate BC
  
  void ECONTEmulation::convertToElinkData(
    unsigned bx, const TPGFEDataformat::TcRawDataPacket &tcrdp, uint32_t *ve
  ) 
  {
    
    const std::vector<TPGFEDataformat::TcRawData>& vtc = tcrdp.getTcData();
    
    unsigned nVe(0);
    

    
    uint64_t data(uint64_t(bx)<<60);
    unsigned last(60);
    unsigned w(0);
    
    TPGFEDataformat::Type tcType(tcrdp.type());
    bool bcMap(false);
    uint64_t bcBits(0);
    
    if(tcType==TPGFEDataformat::BestC) {
      //assert(vtc[0].isModuleSum());
      data|=(uint64_t(tcrdp.moduleSum())<<52);
      last-=8;
      w++;
      
      bcMap=(vtc.size()>7); //High occupancy if nTC>=8
      
      ////////////// construct map for high occupancy ///////////////
      if(bcMap) {
        for(unsigned i(0);i<vtc.size();i++) {
          data|=uint64_t(1)<<(51-vtc[i].address());
        }
        last-=48;
      }
      ////////////////////////////////////////////
    }
    
    if(!bcMap) {
      for(w = 0;w<vtc.size() && !bcMap;w++) {
        if(last<32) {
          ve[nVe++]=data>>32;
          data=(data<<32);
          last+=32;
        }

        if     (tcType==TPGFEDataformat::BestC) last-=6;
        else if(tcType==TPGFEDataformat::STC4A) last-=2;
        else if(tcType==TPGFEDataformat::STC4B) last-=2;
        else if(tcType==TPGFEDataformat::CTC4A) last-=0;
        else if(tcType==TPGFEDataformat::CTC4B) last-=0;
        else if(tcType==TPGFEDataformat::STC16) last-=4;
        else assert(false);

	if(tcType!=TPGFEDataformat::CTC4A and tcType!=TPGFEDataformat::CTC4B)
	  data|=(uint64_t(vtc[w].address())<<last);
      }
    }
    
    //unsigned wLo(tcType==TPGFEDataformat::BestC?1:0);
    for(unsigned w(0);w<vtc.size();w++) {
      if(last<32) {
        ve[nVe++]=data>>32;
        data=(data<<32);
        last+=32;
      }

      if     (tcType==TPGFEDataformat::BestC) last-=7;
      else if(tcType==TPGFEDataformat::STC4A) last-=7;
      else if(tcType==TPGFEDataformat::STC4B) last-=9;
      else if(tcType==TPGFEDataformat::CTC4A) last-=7;
      else if(tcType==TPGFEDataformat::CTC4B) last-=9;
      else if(tcType==TPGFEDataformat::STC16) last-=9;
      else assert(false);

      data|=(uint64_t(vtc[w].energy())<<last);
    }
    
    // Catch last words
    if(last<32) {
      ve[nVe++]=data>>32;
      data=(data<<32);
      last+=32;
    }
    
    ve[nVe++]=data>>32;
  }
  
  void ECONTEmulation::getElinkData(unsigned bx, uint32_t *ptr) const {
    convertToElinkData(bx,emulOut.second,ptr);
  }

  void ECONTEmulation::fillRandomTcRawData(unsigned bx) {
    // FIXME - HOW TO GET TYPE AND NUMBER OF TCS?
    //generateRandomTcRawData(bx,configs.getEconTPara().at(moduleId).getOutType(),9,emulOut);
  }

  void ECONTEmulation::fillZeroEnergyTcRawData(unsigned bx) {
    // FIXME - HOW TO GET TYPE AND NUMBER OF TCS?
    //generateZeroEnergyTcRawData(bx,configs.getEconTPara().at(moduleId).getOutType(),9,emulOut);
  }

  void ECONTEmulation::generateRandomTcRawData(unsigned bx,
					       TPGFEDataformat::Type type,
					       unsigned nTc,
					       TPGFEDataformat::TcRawDataPacket &vtcrp) {
    generateTcRawData(false,bx,type,nTc,vtcrp);
  }
  
  void ECONTEmulation::generateZeroEnergyTcRawData(unsigned bx,
						   TPGFEDataformat::Type type,
						   unsigned nTc,
						   TPGFEDataformat::TcRawDataPacket &vtcrp) {

    generateTcRawData(true,bx,type,nTc,vtcrp);
  }
  
  void ECONTEmulation::generateTcRawData(bool zero, unsigned bx,
					 TPGFEDataformat::Type type,
					 unsigned nTc,
					 TPGFEDataformat::TcRawDataPacket &vtcrp) {
    
    //std::vector<TPGFEDataformat::TcRawData> &vtc(vtcrp.second);
    std::vector<TPGFEDataformat::TcRawData> &vtc(vtcrp.setTcData());
    vtcrp.setType(type);
  
    if(type==TPGFEDataformat::BestC) {
      vtc.resize(nTc);
      vtcrp.setModuleSum(zero?0:rand()&0xff);
    
      unsigned step(48/nTc);
      //for(unsigned i(1);i<vtc.size();i++) {
      for(unsigned i(0);i<vtc.size();i++) {
	      vtc[i].setTriggerCell(type,step*(i)+(rand()%step),zero?0:rand()&0x7f);
      }
    
    } else if(type==TPGFEDataformat::STC4A) {
      vtc.resize(nTc);
      for(unsigned i(0);i<vtc.size();i++) {
	vtc[i].setTriggerCell(type,rand()&0x3,zero?0:rand()&0x7f);
      }
    
    } else if(type==TPGFEDataformat::STC4B) {
      vtc.resize(nTc);
      for(unsigned i(0);i<vtc.size();i++) {
	vtc[i].setTriggerCell(type,rand()&0x3,zero?0:rand()&0x1ff);
      }
    
    } else if(type==TPGFEDataformat::STC16) {
      vtc.resize(nTc);
      for(unsigned i(0);i<vtc.size();i++) {
	vtc[i].setTriggerCell(type,rand()&0xf,zero?0:rand()&0x1ff);
      }
    
    } else {
      assert(false);
    }
  }

  
}//end of namespace


#endif
