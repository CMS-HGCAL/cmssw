/*
 * Authors:
 *   Yulun Miao, Northwestern University (primary)
 *   J. T. Offermann, Brown University
 */

#ifndef CondFormats_HGCalObjects_HGCalTriggerConfiguration_h
#define CondFormats_HGCalObjects_HGCalTriggerConfiguration_h
#include "CondFormats/Serialization/interface/Serializable.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "DataFormats/HGCalDigi/interface/HGCalTriggerDefinitions.h"
#include <map>
#include <vector>
#include <iomanip>
#include <cassert>
#include <cmath>

// @short configuration for HGCROC
struct HGCalROCTrigConfig {
  uint8_t ped[36] = {0};
  uint8_t adc_th;   //5 bits adc th global for HGCROC &0x1F
  // FIXME read from cfg, atm dummy values
  uint64_t ClrAdcTot_trig = 0;  //36-bits 
  uint8_t MultFactor = 1; //5-bits
  uint8_t Tot_P[4] = {0};  //one per 9 channel (each with 7-bits)
  uint8_t Tot_TH[4] = {0}; //one per 9 channel (each with 8 bits)

  bool isChMasked(uint32_t ich) const {
    int chnl = ich%36;
    return (ClrAdcTot_trig>>chnl) & 0x1 ;
  }
  uint32_t getTotTH(uint32_t ich) const {
    uint32_t chnl = ich%36;
    uint32_t  tot_idx = std::floor(chnl/9);
    return uint32_t(Tot_TH[tot_idx]);
  }
  uint32_t getTotP(uint32_t ich) const {
    uint32_t chnl = ich%36;
    uint32_t tot_idx = std::floor(chnl/9);
    return uint32_t(Tot_P[tot_idx]);
  }

  COND_SERIALIZABLE;
};
// @short configuration for ECON-T module
struct HGCalECONTConfig {
  uint8_t density; //lsb at the input TC from ROC
  uint8_t dropLSB; //lsb at the output during the packing
  uint8_t select; //0 = Threshold Sum (TS), 1 = Super Trigger Cell (STC), 2 = Best Choice (BC), 3 = Repeater, 4=Autoencoder (AE).
  uint8_t stcType; //0 = STC4B(5E+4M), 1 = STC16(5E+4M), 2 = CTC4A(4E+3M), 3 = STC4A(4E+3M), 4 = CTC4B(5E+3M)
  uint8_t eportTxNumen; //number of elinks
  uint8_t sumType;  //0: MS summed over 48-N TCs, 1: MS summed over 48 TCs
  std::vector<uint16_t> calv; //12-bit calibration for 48 TCs
  std::vector<uint8_t> tcMux;   //multiplexer between HGCROC and TC to ECONT
  std::vector<uint32_t> offset;
  std::vector<HGCalROCTrigConfig> hgcrocs;
  
  uint32_t getNofTCs() const { return getBCType(); }
  uint32_t getBCType() const {
    uint32_t maxTcs = 0;
    if(getOutType()==TPGFEDataformat::BestC){
      switch(eportTxNumen){
      case 1:
        maxTcs = 1;
        break;
      case 2:
        maxTcs = 4;
        break;
      case 3:
        maxTcs = 6;
        break;
      case 4:
        maxTcs = 9;
        break;
      case 5:
        maxTcs = 14;
        break;
      case 6:
        maxTcs = 18;
        break;
      case 7:
        maxTcs = 23;
        break;
      case 8:
        maxTcs = 28;
        break;
      case 9:
        maxTcs = 32;
        break;
      case 10:
        maxTcs = 37;
        break;
      case 11:
        maxTcs = 41;
        break;
      case 12:
        maxTcs = 46;
        break;
      default: //same as case13 or above
        maxTcs = 48;
        break;
      }
    }
    return maxTcs;
  }

  TPGFEDataformat::Type getOutType() const {
    TPGFEDataformat::Type type;
    switch(select){
      case 0:
        type = TPGFEDataformat::TS;
        break;
      case 1:
        switch(stcType){
          case 0:
            type = TPGFEDataformat::STC4B;
            break;
          case 1:
            type = TPGFEDataformat::STC16;
            break;
          case 2:
            type = TPGFEDataformat::CTC4A;
            break;
          case 3:
            type = TPGFEDataformat::STC4A;
            break;
          case 4:
            type = TPGFEDataformat::CTC4B;
            break;
          default:
            type = TPGFEDataformat::Unknown;
            break;
        }
        break;
      case 2:
        type = TPGFEDataformat::BestC;
        break;
      case 3:
        type = TPGFEDataformat::RA;
        break;
      case 4:
        type = TPGFEDataformat::AE;
        break;
      default:
        type = TPGFEDataformat::Unknown;
        break;
    }
    return type;
  }

  uint32_t getNofSTCs() const {
    uint32_t maxSTCs = 0;
    switch(stcType){
      case 0: //0 = STC4B(5E+4M)
      switch(eportTxNumen){
        case 1:
          maxSTCs = 2;
          break;
        case 2:
          maxSTCs = 5;
          break;
        case 3:
          maxSTCs = 8;
          break;
        case 4:
          maxSTCs = 11;
          break;
        default: //case 5 or higher
          maxSTCs = 12;
          break;
      }
      break;
      case 1: //1 = STC16(5E+4M)
      switch(eportTxNumen){
        case 1:
          maxSTCs = 2;
          break;
        default: //case 2 or higher
          maxSTCs = 3;
          break;
      }
      break;
      case 2: //2 = CTC4A(4E+3M)
      switch(eportTxNumen){
        case 1:
          maxSTCs = 4;
          break;
        case 2:
          maxSTCs = 8;
          break;
        default: //case 3 or higher
          maxSTCs = 12;
          break;
      }
      break;
      default: //3 = STC4A(4E+3M) //4 = CTC4B(5E+3M)
      switch(eportTxNumen){
        case 1:
          maxSTCs = 3;
          break;
        case 2:
          maxSTCs = 6;
          break;
        case 3:
          maxSTCs = 10;
          break;
        default: //case 4 or higher
          maxSTCs = 12;
          break;
      }
    }//stctype
    return maxSTCs;
  }

  uint32_t getCalibration(uint32_t itc) const {
    assert(itc<=47) ;
    return uint32_t(calv.at(itc) & 0xFFF);
  }

  uint32_t getInputMux(uint32_t itc) const {
    assert(itc<=47) ;
    return (isConnectedMux(itc)) ? uint32_t(tcMux.at(itc)) : 0x80 ;
  }

  bool isConnectedMux(uint32_t itc) const {
    assert(itc<=47) ;
    return (tcMux.at(itc)>>7) ? false : true ;
  }

  COND_SERIALIZABLE;

};

// @short configuration for TDAQ block
struct HGCalTDAQConfig {
  uint32_t tdaqBlockHeaderMarker;  // begin of event marker/identifier for
  // nBX should be dynamically read from the TDAQ header
  std::vector<HGCalECONTConfig> econts; // if size==0, then this TDAQ block is skipped
  COND_SERIALIZABLE;
};

// @short configuration for FED
struct HGCalTriggerFedConfig {
  std::vector<HGCalTDAQConfig> tdaqs;
  std::vector<int32_t> econtSwapOffset;
  std::map<uint8_t, std::vector<uint8_t>> elinksMap;
  // TODO: if we want to add information checking for S-Link header/trailer
  COND_SERIALIZABLE;
};

/**
 *  @short Main HGCal configuration with a tree structure of vectors of
 *         HGCalTriggerFedConfig/HGCalTriggerFedConfig/HGCalTDAQConfig structs as follows:
 *         config.feds[dense_fed_idx].tdaq[dense_tdaq_idx].econt[dense_econt_idx]
 **/
class HGCalTriggerConfiguration {
public:
  std::vector<HGCalTriggerFedConfig> feds;
private:
  COND_SERIALIZABLE;
};

inline std::ostream& operator<<(std::ostream& os, const HGCalTriggerConfiguration& config) {
  std::size_t nfed = config.feds.size();
  os << "HGCalConfiguration( nfed=" << std::dec << nfed << std::endl;
  for (std::size_t ifed = 0; ifed < nfed; ifed++) {
    HGCalTriggerFedConfig fedConfig = config.feds[ifed];
    os << "fed[" << std::dec << ifed << "] with " <<std::dec << fedConfig.tdaqs.size() << " TDAQ blocks" << std::endl
       << "SwapOffset=[";
    for (std::size_t iecont = 0; iecont < fedConfig.econtSwapOffset.size(); iecont++){
      os << fedConfig.econtSwapOffset[iecont] <<", ";
    }
    os << "]," << std::endl;
    for (std::size_t itdaq = 0; itdaq < fedConfig.tdaqs.size(); itdaq++) {
      HGCalTDAQConfig tdaqConfig = fedConfig.tdaqs[itdaq];
      os << "fed[" << std::dec << ifed << "].tdaq[" << itdaq
         << "], headerMarker = 0x" << std::hex << std::setfill('0') << std::setw(8) << tdaqConfig.tdaqBlockHeaderMarker << std::endl;
      if (tdaqConfig.econts.size()==0) {
        os << std::dec << "with no active ECON-Ts, skipped" << std::endl;
        continue;
      }
      os << " with " << std::dec << tdaqConfig.econts.size() << " active ECON-Ts" << std::endl;
      for(unsigned int iecont=0; iecont<tdaqConfig.econts.size(); iecont++){
        HGCalECONTConfig econtConfig = tdaqConfig.econts[iecont];
        os << "fed[" << std::dec << ifed << "].tdaq[" << itdaq << "].econt[" << iecont << "], density = " << std::dec << (int)econtConfig.density
           << ", dropLSB = " << std::dec << (int)econtConfig.dropLSB
           << ", select = " << std::dec << (int)econtConfig.select
           << ", stcType = " << std::dec << (int)econtConfig.stcType
           << ", eportTxNumen = " << std::dec << (int)econtConfig.eportTxNumen
           << ", sumType = " << std::dec << (int)econtConfig.sumType<< "," <<std::endl
           << "calv = [";
        for(unsigned int i=0; i<econtConfig.calv.size(); i++){
          os << "(0x" << std::hex << std::setfill('0') << std::setw(3) << econtConfig.calv[i] << ", " <<std::dec<< (float)econtConfig.calv[i]/2048 << "), ";
        }
        os << "]," << std::endl
           << "mux = [";
        for(unsigned int i=0; i<econtConfig.tcMux.size(); i++){
          os << std::dec << (int)econtConfig.tcMux[i]<<", ";
        }
        os << "]," << std::endl
           << "offset = [";
        for(unsigned int i=0; i<econtConfig.offset.size(); i++){
          os << std::dec << (int)econtConfig.offset[i]<<", ";
        }
        os << std::endl;
        if (econtConfig.hgcrocs.size()==0) {
          os << "config for HGCROC not loaded, skipped" << std::endl;
        continue;
        }
        os << " with " << std::dec << econtConfig.hgcrocs.size() << " HGCROCs" << std::endl;
        for (unsigned int ihgcroc=0; ihgcroc<econtConfig.hgcrocs.size(); ihgcroc++){
          HGCalROCTrigConfig hgcrocConfig = econtConfig.hgcrocs[ihgcroc];

          os << "fed[" << std::dec << ifed << "].tdaq[" << itdaq << "].econt[" << iecont << "].hgcroc[" << ihgcroc << "], ADC th = "
             << std::dec << (int)hgcrocConfig.adc_th << std::endl;

        }
      }
    }
  }
  return os;
}
// NOTE: Moved this out of the "TPGFEConfiguration" namespace - Jan
inline uint16_t getExpEcontHeader(uint16_t BXSLink, TPGFEDataformat::Type type ){ 	//expected ECONT header from Slink BX counter
  uint16_t expEcontHeader = 0;
  uint16_t  maxMSB = 0;
  uint16_t  modEcontHeader = 1;

  if (type == TPGFEDataformat::TS){ // 5 bits ECONT header
    maxMSB = 31;
    modEcontHeader = 16;
  }
  else{ // 4 bits ECONT header
    maxMSB = 15;
    modEcontHeader = 8;

  }

  if (BXSLink == 1) {
    expEcontHeader = maxMSB;
  }
  else expEcontHeader = ((BXSLink - 1) % modEcontHeader);
  return expEcontHeader;
}


#endif
