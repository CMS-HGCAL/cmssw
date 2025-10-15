#include "EventFilter/HGCalRawToDigi/interface/HGCalUnpackerTrigger.h"
#include <iostream>
#include <iomanip>

using namespace hgcal;

bool HGCalUnpackerTrigger::parseFEDData(unsigned fedId,
                                        const RawFragmentWrapper& fed_data,
                                        const HGCalTriggerConfiguration& config,
                                        const HGCalMappingModuleIndexerTrigger& moduleIndexer,
                                        hgcaldigi::HGCalDigiTriggerHost& digisTrigger) {
  
  // Endianness assumption
  // From 32-bit word(ECOND) to 64-bit word(capture block): little endianness
  // Others: big endianness

  // TODO: if this also depends on the unpacking configuration, it should be moved to the specialization
  const auto& fedConfig = config.feds[fedId];
  const auto* start_fed_data = &(fed_data.data().front());
  const auto* const header = reinterpret_cast<const uint64_t*>(start_fed_data);
  const auto* const trailer = reinterpret_cast<const uint64_t*>(start_fed_data + fed_data.size());
  //LogDebug("[HGCalUnpackerTrigger]") << " nwords (64b) = " << std::distance(header, trailer) << "\n";
  std::cout << "HGCalUnpackerTrigger::parseFEDData " << " nwords (64b) = " << std::distance(header, trailer) << "\n";

  //std::vector<uint64_t> words;
  // auto* ptr = header;
  // for (unsigned iword = 0; ptr < trailer; ++iword) {
  //   std::cout << "HGCalUnpackerTrigger::parseFEDData-tword " << std::setw(8) << iword << ": 0x" << std::hex << std::setfill('0')
  //   //LogDebug("[HGCalUnpackerTrigger]") << std::setw(8) << iword << ": 0x" << std::hex << std::setfill('0')
  //                                      << std::setw(16) << *ptr << " (" << std::setfill('0') << std::setw(8)
  //                                      << *(reinterpret_cast<const uint32_t*>(ptr) + 1) << " " << std::setfill('0')
  //                                      << std::setw(8) << *reinterpret_cast<const uint32_t*>(ptr) << ")\n"
  //                                      << std::dec;
  //   ++ptr;
  // }
  
  const uint64_t* ptr = header;
  char num[10], word64[20], word32m[20], word32l[20];
  for (unsigned iword = 0; ptr < trailer; ++iword) {
    uint64_t tword = *ptr;
    uint32_t tword32m = ((tword>>32) & 0xffffffff);
    uint32_t tword32l = tword & 0xffffffff;
    sprintf(num,"%03u",iword);
    sprintf(word64,"0x%016lx",tword);
    sprintf(word32m,"0x%08x",tword32m);
    sprintf(word32l,"0x%08x",tword32l);
    std::cout << "HGCalUnpackerTrigger::parseFEDData::tword " << num << " " << word64  << " (" << word32m << ", " << word32l << ")" << std::endl;    
    ++ptr;
  }
  
  // ptr = header+2; // skip S-Link header
  // unsigned int TDAQblockIdx = 0;
  // while (ptr < trailer-3){ // -3 to skip S-Link trailer and possible padding before S-Link trailer
  //   // Parse the FEDRaw data into TDAQ blocks
  //   HGCalTDAQConfig tdaqConfig=fedConfig.tdaqs[TDAQblockIdx];
  //   uint64_t tdaq_header = *ptr;
  //   if (tdaq_header >> TDAQ_FRAME::TDAQ_HEADER_POS != tdaqConfig.tdaqBlockHeaderMarker){
  //     // If the TDAQ header marker is not found, return with error
  //     return false;
  //   }
  //   auto* tdaq_body_start = ptr + 1;
  //   uint32_t tdaq_body_length = ((tdaq_header >> TDAQ_FRAME::TDAQ_PKT_LENGTH_POS) & TDAQ_FRAME::TDAQ_PKT_LENGTH_MASK) - 1;
  //   ptr += tdaq_body_length + 1;// move to the next TDAQ block
  //   TDAQblockIdx++;// move to the next TDAQ block
  //   if(tdaqConfig.econts.size()==0) continue;
  //   parseTDAQBlock();
  // }
  
  return true;
}

