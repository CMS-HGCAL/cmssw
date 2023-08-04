/****************************************************************************
 *
 * This is a part of HGCAL offline software.
 * Authors:
 *   Yulun Miao, Northwestern University
 *   Huilin Qu, CERN
 *   Laurent Forthomme, CERN
 *
 ****************************************************************************/

#include "EventFilter/HGCalRawToDigi/interface/HGCalUnpacker.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

HGCalUnpacker::HGCalUnpacker(HGCalUnpackerConfig config)
    : config_(config),
      channelData_(config_.channelMax),
      commonModeSum_(config_.channelMax),
      commonModeData_(config_.commonModeMax) {}

void HGCalUnpacker::parseSLink(
    const std::vector<uint32_t>& inputArray0,
    const std::function<uint16_t(uint16_t sLink, uint8_t captureBlock, uint8_t econd)>& enabledERXMapping,
    const std::function<uint16_t(uint16_t fedid)>& fed2slink) {
  uint16_t sLink;
  bool zside(false); //fixme: this should be based on slink number (fedId)
  channelDataSize_ = 0;
  commonModeDataSize_ = 0;
  flaggedECOND_.clear();

  std::vector<uint32_t> inputArray(inputArray0); // FIXME
  for (uint32_t iword = 0; iword < inputArray.size();) {  // loop through the S-Link

    //----- parse the S-Link header which is 128b long
    //----- 32b come ordered as
    //          first word  [b'32-73]
    //          second word [b'0-31]
    //          third word  [b'96-127]
    //          fourth word [b'64-95]
    LogDebug("[HGCalUnpacker::parseSLink]") << std::hex << inputArray[iword] << " "
                                            << inputArray[iword+1] << " | "
                                            << inputArray[iword+2] << " "
                                            << inputArray[iword+3];

    if (config_.applyFWworkaround) {
      std::swap(inputArray[iword], inputArray[iword + 1]);
      std::swap(inputArray[iword + 2], inputArray[iword + 3]);
    }
    if (((inputArray[iword+2] >> kSLinkBOEShift) & kSLinkBOEMask) != config_.sLinkBOE)  // sanity check
      throw cms::Exception("CorruptData")
        << "Expected a S-Link header at word "
        << std::dec << iword+2 << " (BOE: 0x" << config_.sLinkBOE << "), got 0x"
        << std::hex << inputArray[iword+2] << ".";

    uint16_t fedid=((inputArray[iword+1]>>kSLinkFEDIdShift)&kSLinkFEDIdMask); 
    sLink=fed2slink(fedid);
    LogDebug("[HGCalUnpacker::parseSLink]") << "SLink=" << sLink << " index assigned from FED ID=" << fedid;

    
    iword += 4;  // length of the S-Link header (128 bits)

    //----- parse the S-Link body
    for (uint8_t captureBlock = 0; captureBlock < config_.sLinkCaptureBlockMax;
         captureBlock++) {  // loop through all capture blocks
      //----- parse the capture block header
      if (config_.applyFWworkaround) {
        std::swap(inputArray[iword], inputArray[iword + 1]);
      }
      uint32_t cb_msb=inputArray[iword];      
      if (((cb_msb >> kCaptureBlockReservedShift) & kCaptureBlockReservedMask) !=
          config_.cbHeaderMarker)  // sanity check
        throw cms::Exception("CorruptData")
            << "Expected a capture block header at word " << std::dec << iword << "/0x" << std::hex << iword
            << " (reserved word: 0x" << config_.cbHeaderMarker << "), got 0x"
            << ((cb_msb >> kCaptureBlockReservedShift) & kCaptureBlockReservedMask)
            << " from 0x" << cb_msb  << ".";
      uint32_t cb_lsb=inputArray[iword+1];
      
      const uint64_t captureBlockHeader = ((uint64_t)cb_msb << 32) | ((uint64_t)cb_lsb);
      iword += 2;  // length of capture block header (64 bits)
      LogDebug("[HGCalUnpacker::parseSLink]") << "Capture block=" << (int)captureBlock << ", capture block header=0x" << std::hex << captureBlockHeader;
      //----- parse the capture block body
      for (uint8_t econd = 0; econd < config_.captureBlockECONDMax; econd++) {  // loop through all ECON-Ds

        auto econd_status_flag = ((captureBlockHeader >> (3 * econd)) & kCaptureBlockECONDStatusMask);
        if (config_.applyFWworkaround) {
          if (econd_status_flag == 0b111 || econd_status_flag == 0b001)
            continue;
        } else {
          if (econd_status_flag >= 0b100)
            continue;  // only pick active ECON-Ds
        }

        HGCalElectronicsId eleid(zside, sLink, captureBlock, econd, 0, 0);
        LogDebug("[HGCalUnpacker::parseSLink]") << std::dec << (uint32_t)zside << " " << (uint32_t)sLink << " " << (uint32_t)captureBlock << " " << (uint32_t)econd << std::endl;
        // //due to a bug in the firmware there is one extra 64b word after the CB which is to be ignored
        // //this will skip it but should be removed once the bug is fixed in the firmware
        // if(config_.applyFWworkaround) {
        //   LogDebug("[HGCalUnpacker::parseSLink]") << std::dec << (uint32_t)(econd) << " is active skipping extra 64b bugged word"
        //                                           << std::hex << inputArray[iword-2] << " " << inputArray[iword-1] << std::endl
        //                                           << std::hex << inputArray[iword] << " " << inputArray[iword+1] << " <--- BE feature" << std::endl
        //                                           << std::hex << inputArray[iword+2] << " " << inputArray[iword+3] << " <--- ECON-D" << std::endl;
        //   iword +=2;
        // }
        
        //----- parse the ECON-D header
        // (the second word of ECON-D header contains no information for unpacking, use only the first one)
        if (config_.applyFWworkaround) {
          std::swap(inputArray[iword], inputArray[iword + 1]);
        }
        if (((inputArray[iword] >> kHeaderShift) & kHeaderMask) != config_.econdHeaderMarker) {
          flaggedECOND_.emplace_back(HGCalFlaggedECONDInfo(iword,HGCalFlaggedECONDInfo::WRONGHEADERMARKER,eleid.raw()));
          throw cms::Exception("CorruptData")
            << "Expected a ECON-D header at word " << std::dec << iword << "/0x" << std::hex << iword
            << " (marker: 0x" << config_.econdHeaderMarker << "), got 0x" << inputArray[iword] << ".";
        }

        const auto& econdHeader = inputArray[iword];
        iword += 2;  // length of ECON-D header (2 * 32 bits)

        //----- extract the payload length
        const uint32_t payloadLength = (econdHeader >> kPayloadLengthShift) & kPayloadLengthMask;
        LogDebug("[HGCalUnpacker::parseSLink]") << "ECON-D #" << (int)econd << ", first word of ECON-D header=0x" << std::hex << econdHeader
                                                << "\t ECON-D payload=" << std::dec << payloadLength;
        // if payload length too big
        if (payloadLength > config_.payloadLengthMax) {
          flaggedECOND_.emplace_back(HGCalFlaggedECONDInfo(iword,HGCalFlaggedECONDInfo::PAYLOADOVERFLOWS,eleid.raw()));
          throw cms::Exception("CorruptData") << "Unpacked payload length=" << payloadLength
                                              << " exceeds the maximal length=" << config_.payloadLengthMax;
        }
        LogDebug("[HGCalUnpacker::parseSLink]") << "ECON-D #" << (int)econd << ", payload length=" << payloadLength;

        //
        // Quality check for the ECON-D
        //
	bool isTruncated(((econdHeader >> kTruncatedShift) & kTruncatedMask) == 1);
        uint32_t econdQuality(
                              (((captureBlockHeader >> (3 * econd)) & kCaptureBlockECONDStatusMask) != 0b000)*HGCalFlaggedECONDInfo::CBSTATUS + 
                              (((econdHeader >> kHTShift) & kHTMask) >= 0b10)*HGCalFlaggedECONDInfo::HTBITS +
                              (((econdHeader >> kEBOShift) & kEBOMask) >= 0b10)*HGCalFlaggedECONDInfo::EBOBITS +
                              (((econdHeader >> kMatchShift) & kMatchMask) == 0)*HGCalFlaggedECONDInfo::MATCHBIT +
                              (isTruncated*HGCalFlaggedECONDInfo::TRUNCATED) );
        if(econdQuality>0) {
          flaggedECOND_.emplace_back(HGCalFlaggedECONDInfo(iword - 2,econdQuality,eleid.raw()));          
          LogDebug("[HGCalUnpacker::parseSLink]") << "ECON-D failed quality check, HT=" << (econdHeader >> kHTShift & kHTMask)
                                  << ", EBO=" << (econdHeader >> kEBOShift & kEBOMask)
                                  << ", M=" << (econdHeader >> kMatchShift & kMatchMask)
                                  << ", T=" << (econdHeader >> kTruncatedShift & kTruncatedMask);
        }
        
        //if the ECON-D is truncated skip directly to the next
        if(isTruncated) {
          iword += payloadLength;  // skip the current ECON-D (using the payload length parsed above)
          if (iword % 2 != 0) {  //TODO: check this
            LogDebug("[HGCalUnpacker::parseSLink]") << "Padding ECON-D payload to 2 32-bit words (remainder: " << (iword % 2) << ").";
            iword += 1;
          }
          continue;  
        }
 
        const uint32_t econdBodyStart = iword;  // for the ECON-D length check
        //----- parse the ECON-D body NORMAL MODE
        if (((econdHeader >> kPassThroughShift) & kPassThroughMask) == 0) {
          // standard ECON-D
          LogDebug("[HGCalUnpacker::parseSLink]") << "Standard ECON-D";
          const auto enabledERX = enabledERXMapping(sLink, captureBlock, econd);
          for (uint8_t erx = 0; erx < config_.econdERXMax; erx++) {
            
            //loop through eRx
            //pick active eRx
            if ((enabledERX >> erx & 1) == 0)
              continue;
            
            //----- parse the eRX subpacket header
            //common mode
            const HGCalElectronicsId cm0id(zside, sLink, captureBlock, econd, erx, 37);
            const HGCalElectronicsId cm1id(zside, sLink, captureBlock, econd, erx, 38);
            LogDebug("[HGCalUnpacker::parseSLink]") << "ECON-D:eRx=" << (int)econd << ":" << (int)erx
                                    << ", first word of the eRx header=0x" << std::hex << inputArray[iword] << "\n"
                                    << "  extracted common mode 0 HGCalElectronicsId=" << std::dec<<cm0id.raw() << ",adc=0x" << std::hex
                                    << ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) << "\n"
                                    << "  extracted common mode 1 HGCalElectronicsId=" << std::dec<<cm1id.raw() << ",adc=0x" << std::hex
                                    << ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask) << "\n";
            //assume common mode in char mode
            commonModeData_[commonModeDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
                                                                                              cm0id,
                                                                                              ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask)<<10);
            commonModeData_[commonModeDataSize_ + 1] = HGCROCChannelDataFrame<HGCalElectronicsId>(
                                                                                                  cm1id,
                                                                                                  ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask)<<10);
            uint16_t cmSum = ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) + ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask);
            if (commonModeDataSize_ + 1 > config_.commonModeMax)
              throw cms::Exception("HGCalUnpack") << "Too many common mode data unpacked: " << (commonModeDataSize_ + 1)
                                                  << " >= " << config_.commonModeMax << ".";
            commonModeDataSize_ += 2;
            // empty check
            if (((inputArray[iword] >> kFormatShift) & kFormatMask) == 1) {  // empty
              LogDebug("[HGCalUnpacker::parseSLink]") << "eRx empty";
              iword += 1;  // length of an empty eRx header (32 bits)
              continue;    // go to the next eRx
            }
            // regular mode
            const uint64_t erxHeader = ((uint64_t)inputArray[iword] << 32) | ((uint64_t)inputArray[iword + 1]);
            iword += 2;  // length of a standard eRx header (2 * 32 bits)
            LogDebug("[HGCalUnpacker::parseSLink]") << "whole eRx header=0x" << std::hex << erxHeader;
            
            //----- parse the eRx subpacket body
            uint32_t bitCounter = 0;
            for (uint8_t channel = 0; channel < config_.erxChannelMax; channel++) {  // loop through all channels in eRx
              if (((erxHeader >> channel) & 1) == 0)
                continue;  // only pick active channels
              const HGCalElectronicsId id(zside, sLink, captureBlock, econd, erx, channel);
              const uint32_t tempIndex = bitCounter / 32 + iword;
              const uint8_t tempBit = bitCounter % 32;
              const uint32_t temp =
                (tempBit == 0) ? inputArray[tempIndex]
                : (inputArray[tempIndex] << tempBit) | (inputArray[tempIndex + 1] >> (32 - tempBit));
              const uint8_t code = temp >> 28;
              // use if and else here
              channelData_[channelDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
                                                                                          id,
                                                                                          ((temp << erxBodyLeftShift_[code]) >> erxBodyRightShift_[code]) & erxBodyMask_[code]);
              bitCounter += erxBodyBits_[code];
              if (code == 0b0010)
                channelData_[channelDataSize_].fillFlag1(1);
              commonModeSum_[channelDataSize_]=cmSum;
              LogDebug("[HGCalUnpacker::parseSLink]") << "Word " << channelDataSize_ << ", ECON-D:eRx:channel=" << (int)econd << ":"
                                      << (int)erx << ":" << (int)channel << "\n"
                                      << "  full word readout=0x" << std::hex << temp << std::dec << ", code=0x"
                                      << std::hex << (int)code << std::dec << "\n"
                                      << "  extracted channel data=0x" << std::hex
                                      << channelData_[channelDataSize_].raw();
              channelDataSize_++;
            }

            // pad to the whole word
            iword += bitCounter / 32;
            if (bitCounter % 32 != 0)
              iword += 1;            
          }

        }
        
        // passthrough ECON-D
        else {
          
          LogDebug("[HGCalUnpacker::parseSLink]") << "Passthrough ECON-D";

          const auto enabledERX = enabledERXMapping(sLink, captureBlock, econd);
          for(uint8_t erx = 0; erx < config_.econdERXMax; erx++) {  // loop through all eRxs

            // only pick active eRxs
            if ((enabledERX >> erx & 1) == 0)
              continue;  

            //check if e-RX is empty and skip it
            bool emptyERX( (inputArray[iword] & 0x1f) == 0b10000);
            if(emptyERX) {
              LogDebug("[HGCalUnpacker::parseSLink]") << "e-RX is empty...skipping";
              iword +=1;
              continue;
            }

            //----- parse the eRX subpacket header
            const uint64_t erxHeader = ((uint64_t)inputArray[iword] << 32) | ((uint64_t)inputArray[iword + 1]);
            LogDebug("[HGCalUnpacker::parseSLink]") << "e-RX=" << std::dec << erx << " header=0x" << std::hex << erxHeader;
            
            //common mode
            const HGCalElectronicsId cm0id(zside, sLink, captureBlock, econd, erx, 37);
            const HGCalElectronicsId cm1id(zside, sLink, captureBlock, econd, erx, 38);
            LogDebug("[HGCalUnpacker::parseSLink]") << "ECON-D:eRx=" << (int)econd << ":" << (int)erx
                                    << ", e-rx header=0x" << std::hex << erxHeader << "\n"
                                    << "  extracted common mode 0 HGCalElectronicsId=" << std::dec<<cm0id.raw() << ",adc=0x" << std::hex
                                    << ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) << "\n"
                                    << "  extracted common mode 1 HGCalElectronicsId=" << std::dec<<cm1id.raw() << ",adc=0x" << std::hex
                                    << ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask) << "\n";

            //read common mode
            commonModeData_[commonModeDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
                                                                                              cm0id,
                                                                                              ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask)<<10);
            commonModeData_[commonModeDataSize_ + 1] = HGCROCChannelDataFrame<HGCalElectronicsId>(
                                                                                                  cm1id,
                                                                                                  ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask)<<10);
            uint16_t cmSum = ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) + ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask);

            //PEDRO : not sure how this could happen so i commented it out
            //if (commonModeDataSize_ + 1 > config_.commonModeMax)
            //  throw cms::Exception("HGCalUnpack") << "Too many common mode data unpacked: " << (commonModeDataSize_ + 1)
            //                                      << " >= " << config_.commonModeMax << ".";
            commonModeDataSize_ += 2;            
            iword += 2;  // length of the standard eRx header (2 * 32 bits)

            
            for (uint8_t channel = 0; channel < config_.erxChannelMax; channel++) {  // loop through all channels in eRx

              bool ison=((erxHeader >> channel) & 0xb);
              if(!ison) {
                LogDebug("[HGCalUnpacker::parseSLink]") << "Skipping channel " << uint32_t(channel) << " in " << uint32_t(econd) << " eRX=" << uint32_t(erx);
                continue;
              }
          
              const HGCalElectronicsId id(zside, sLink, captureBlock, econd, erx, channel);
              channelData_[channelDataSize_] =
                HGCROCChannelDataFrame<HGCalElectronicsId>(id, inputArray[iword]);
              commonModeSum_[channelDataSize_]=cmSum;
              LogDebug("[HGCalUnpacker::parseSLink]") << "Word " << channelDataSize_ << ", ECON-D:eRx:channel=" << (int)econd << ":"
                                                      << (int)erx << ":" << (int)channel << ", HGCalElectronicsId=" << id.raw()
                                                      << "\n"
                                                      << "extracted channel data=0x" << std::hex
                                                      << channelData_.at(channelDataSize_).raw();
              channelDataSize_++;
              iword++;
            }
          }
        }
        
        //----- parse the ECON-D trailer
        // (no information needed from ECON-D trailer in unpacker, skip it)
        iword += 1;  // length of the ECON-D trailer (32 bits CRC)
        
        if (iword - econdBodyStart != payloadLength) {
          
          flaggedECOND_[flaggedECOND_.size()-1].flags += HGCalFlaggedECONDInfo::PAYLOADMISMATCHES;
          
          throw cms::Exception("CorruptData")
            << "Mismatch between unpacked and expected ECON-D #" << (int)econd << " payload length\n"
            << "  unpacked payload length=" << iword - econdBodyStart << "\n"
            << "  expected payload length=" << payloadLength;
        }  

        // pad to 2 words
        if (iword % 2 != 0) {  //TODO: check this
          LogDebug("HGCalUnpacker") << "Padding ECON-D payload to 2 32-bit words (remainder: " << (iword % 2) << ").";
          iword += 1;
        }
        }


      
      //----- Capture block has no trailer
      // pad to 4 words
      //if (iword % 4 != 0) {  //TODO: check this
      //  LogDebug("HGCalUnpacker") << "Padding capture block to 4 32-bit words (remainder: " << (iword % 4) << ").";
      //  iword += 4 - (iword % 4);
      // }
    }

    
    //----- parse the S-Link trailer
    // (no information is needed in unpacker)  
    iword += 4;  // length of the S-Link trailer (128 bits)
  }

  channelData_.resize(channelDataSize_);
  commonModeSum_.resize(channelDataSize_);
  commonModeData_.resize(commonModeDataSize_);
  return;
}

void HGCalUnpacker::parseCaptureBlock(
    const std::vector<uint32_t>& inputArray,
    const std::function<uint16_t(uint16_t sLink, uint8_t captureBlock, uint8_t econd)>& enabledERXMapping) {
  uint16_t sLink = 0;
  bool zside(false); //fixme me this should be based on the slink id
  uint8_t captureBlock = 0;

  channelDataSize_ = 0;
  commonModeDataSize_ = 0;
  flaggedECOND_.clear();

  for (uint32_t iword = 0; iword < inputArray.size();) {  // loop through all capture blocks

    //----- parse the capture block header
    if (((inputArray[iword] >> kCaptureBlockReservedShift) & kCaptureBlockReservedMask) != config_.cbHeaderMarker)
      throw cms::Exception("CorruptData")
          << "Expected a capture block header at word " << std::dec << iword << "/0x" << std::hex << iword
          << " (reserved word: 0x" << config_.cbHeaderMarker << "), got 0x" << inputArray[iword] << ".";

    const uint64_t captureBlockHeader = ((uint64_t)inputArray[iword] << 32) | ((uint64_t)inputArray[iword + 1]);
    LogDebug("HGCalUnpack") << "Capture block=" << (int)captureBlock << ", capture block header=0x" << std::hex
                            << captureBlockHeader;
    iword += 2;  // length of capture block header (64 bits)

    //----- parse the capture block body
    for (uint8_t econd = 0; econd < config_.captureBlockECONDMax; econd++) {  // loop through all ECON-Ds

      if ((captureBlockHeader >> (3 * econd) & kCaptureBlockECONDStatusMask) >= 0b100)
        continue;  // only pick the active ECON-Ds

      //----- parse the ECON-D header
      // (the second word of ECON-D header contains no information useful for unpacking, use only the first one)
      if (((inputArray[iword] >> kHeaderShift) & kHeaderMask) != config_.econdHeaderMarker)  // sanity check
        throw cms::Exception("CorruptData")
            << "Expected a ECON-D header at word " << std::dec << iword << "/0x" << std::hex << iword << " (marker: 0x"
            << config_.econdHeaderMarker << "), got 0x" << inputArray[iword] << ".";

      const uint32_t econdHeader = inputArray[iword];
      iword += 2;  // length of ECON-D header (2 * 32 bits)

      LogDebug("HGCalUnpack") << "ECON-D #" << (int)econd << ", first word of ECON-D header=0x" << std::hex
                              << econdHeader;

      //----- extract the payload length
      const uint32_t payloadLength = ((econdHeader >> kPayloadLengthShift)) & kPayloadLengthMask;     
      if (payloadLength > config_.payloadLengthMax)  // payload length is too large
        throw cms::Exception("CorruptData") << "Unpacked payload length=" << payloadLength
                                            << " exceeds the maximal length=" << config_.payloadLengthMax;
      LogDebug("HGCalUnpack") << "ECON-D #" << (int)econd << ", payload length = " << payloadLength;

      //check econ-d quality
      uint32_t econdQuality(
                            (((captureBlockHeader >> (3 * econd)) & kCaptureBlockECONDStatusMask) != 0b000)*HGCalFlaggedECONDInfo::CBSTATUS + 
                            (((econdHeader >> kHTShift) & kHTMask) >= 0b10)*HGCalFlaggedECONDInfo::HTBITS +
                            (((econdHeader >> kEBOShift) & kEBOMask) >= 0b10)*HGCalFlaggedECONDInfo::EBOBITS +
                            (((econdHeader >> kMatchShift) & kMatchMask) == 0)*HGCalFlaggedECONDInfo::MATCHBIT +
                            (((econdHeader >> kTruncatedShift) & kTruncatedMask) == 1)*HGCalFlaggedECONDInfo::TRUNCATED);
      
      if(econdQuality) {
        LogDebug("HGCalUnpack") << "ECON-D failed quality check, HT=" << (econdHeader >> kHTShift & kHTMask)
                                << ", EBO=" << (econdHeader >> kEBOShift & kEBOMask)
                                << ", M=" << (econdHeader >> kMatchShift & kMatchMask)
                                << ", T=" << (econdHeader >> kTruncatedShift & kTruncatedMask);
        HGCalElectronicsId eleid(zside, sLink, captureBlock, econd, 0, 0);
        flaggedECOND_.emplace_back(HGCalFlaggedECONDInfo(iword - 2,econdQuality,eleid.raw()));
        
        iword += payloadLength;  // skip the current ECON-D (using the payload length parsed above)

        if (iword % 2 != 0) {  //TODO: check this
          LogDebug("HGCalUnpacker") << "Padding ECON-D payload to 2 32-bit words (remainder: " << (iword % 2) << ").";
          iword += 1;
        }
        continue;  // go to the next ECON-D
      }

      //----- parse the ECON-D body
      const uint32_t econdBodyStart = iword;  // for the ECON-D length check
      if (((econdHeader >> kPassThroughShift) & kPassThroughMask) == 0) {
        // standard ECON-D
        LogDebug("HGCalUnpack") << "Standard ECON-D";
        const auto enabledERX = enabledERXMapping(sLink, captureBlock, econd);
        for (uint8_t erx = 0; erx < config_.econdERXMax; erx++) {  // loop through all eRxs
          if ((enabledERX >> erx & 1) == 0)
            continue;  // only pick active eRx

          //----- parse the eRX subpacket header
          //common mode
          const HGCalElectronicsId cm0id(zside, sLink, captureBlock, econd, erx, 37);
          const HGCalElectronicsId cm1id(zside, sLink, captureBlock, econd, erx, 38);
          LogDebug("HGCalUnpack") << "ECON-D:eRx=" << (int)econd << ":" << (int)erx
                                  << ", first word of the eRx header=0x" << std::hex << inputArray[iword] << "\n"
                                  << "  extracted common mode 0 HGCalElectronicsId=" << std::dec<<cm0id.raw() << ",adc=0x" << std::hex
                                  << ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) << "\n"
                                  << "  extracted common mode 1 HGCalElectronicsId=" << std::dec<<cm1id.raw() << ",adc=0x" << std::hex
                                  << ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask) << "\n";
          //assume common mode in char mode
          commonModeData_[commonModeDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
              cm0id,
              ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask)<<10);
          commonModeData_[commonModeDataSize_ + 1] = HGCROCChannelDataFrame<HGCalElectronicsId>(
              cm1id,
              ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask)<<10);
          uint16_t cmSum = ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) + ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask);  
          if (commonModeDataSize_ + 1 > config_.commonModeMax)
            throw cms::Exception("HGCalUnpack") << "Too many common mode data unpacked: " << (commonModeDataSize_ + 1)
                                                << " >= " << config_.commonModeMax << ".";
          commonModeDataSize_ += 2;

          // empty check
          if (((inputArray[iword] >> kFormatShift) & kFormatMask) == 1) {
            iword += 1;  // length of an empty eRx header (32 bits)
            LogDebug("HGCalUnpack") << "eRx #" << (int)erx << " is empty.";
            continue;  // go to next eRx
          }

          // regular mode
          const uint64_t erxHeader = ((uint64_t)inputArray[iword] << 32) | (uint64_t)inputArray[iword + 1];
          LogDebug("HGCalUnpack") << "whole eRx header=0x" << std::hex << erxHeader;
          iword += 2;  // length of a standard eRx header (2 * 32 bits)

          uint32_t bitCounter = 0;
          //----- parse the eRx subpacket body
          for (uint8_t channel = 0; channel < config_.erxChannelMax; channel++) {  // loop through channels in eRx
            if (((erxHeader >> channel) & 1) == 0)
              continue;  // only pick active channels
            const HGCalElectronicsId id(zside, sLink, captureBlock, econd, erx, channel);
            const uint32_t tempIndex = bitCounter / 32 + iword;
            const uint8_t tempBit = bitCounter % 32;
            const uint32_t temp =
                (tempBit == 0) ? inputArray[tempIndex]
                               : (inputArray[tempIndex] << tempBit) | (inputArray[tempIndex + 1] >> (32 - tempBit));
            const uint8_t code = temp >> 28;
            // use if and else here
            channelData_[channelDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
                id,
                ((temp << erxBodyLeftShift_[code]) >> erxBodyRightShift_[code]) & erxBodyMask_[code]);
            bitCounter += erxBodyBits_[code];
            if (code == 0b0010)
              channelData_[channelDataSize_].fillFlag1(1);
            commonModeSum_[channelDataSize_]=cmSum;
            LogDebug("HGCalUnpack") << "Word " << channelDataSize_ << ", ECON-D:eRx:channel=" << (int)econd << ":"
                                    << (int)erx << ":" << (int)channel
                                    << "  full word readout=0x" << std::hex << temp << std::dec << ", code=0x"
                                    << std::hex << (int)code << std::dec << "\n"
                                    << "  extracted channel data=0x" << std::hex
                                    << channelData_[channelDataSize_].raw();
            channelDataSize_++;
          }
          // pad to the whole word
          iword += bitCounter / 32;
          if (bitCounter % 32 != 0)
            iword += 1;
          // eRx subpacket has no trailer
        }
      } else {  // passthrough ECON-D
        LogDebug("HGCalUnpack") << "Passthrough ECON-D";
        const auto enabledERX = enabledERXMapping(sLink, captureBlock, econd);
        for (uint8_t erx = 0; erx < config_.econdERXMax; erx++) {  // loop through all eRx
          if ((enabledERX >> erx & 1) == 0)
            continue;  // only pick active eRx

          //----- parse the eRX subpacket header
          //common mode
          const HGCalElectronicsId cm0id(zside, sLink, captureBlock, econd, erx, 37);
          const HGCalElectronicsId cm1id(zside, sLink, captureBlock, econd, erx, 38);
          LogDebug("HGCalUnpack") << "ECON-D:eRx=" << (int)econd << ":" << (int)erx
                                  << ", first word of the eRx header=0x" << std::hex << inputArray[iword] << "\n"
                                  << "  extracted common mode 0 HGCalElectronicsId=" << std::dec<<cm0id.raw() << ",adc=0x" << std::hex
                                  << ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) << "\n"
                                  << "  extracted common mode 1 HGCalElectronicsId=" << std::dec<<cm1id.raw() << ",adc=0x" << std::hex
                                  << ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask) << "\n";
          //assume common mode in char mode
          commonModeData_[commonModeDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
              cm0id,
              ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask)<<10);
          commonModeData_[commonModeDataSize_ + 1] = HGCROCChannelDataFrame<HGCalElectronicsId>(
              cm1id,
              ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask)<<10);
          uint16_t cmSum = ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) + ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask);
          if (commonModeDataSize_ + 1 > config_.commonModeMax)
            throw cms::Exception("HGCalUnpack") << "Too many common mode data unpacked: " << (commonModeDataSize_ + 1)
                                                << " >= " << config_.commonModeMax << ".";
          commonModeDataSize_ += 2;

          iword += 2;  // length of a standard eRx header (2 * 32 bits)

          for (uint8_t channel = 0; channel < config_.erxChannelMax; channel++) {  // loop through all channels in eRx
            const HGCalElectronicsId id(zside, sLink, captureBlock, econd, erx, channel);
            channelData_[channelDataSize_] =
                HGCROCChannelDataFrame<HGCalElectronicsId>(id, inputArray[iword]);
            commonModeSum_[channelDataSize_]=cmSum;
            LogDebug("HGCalUnpack") << "Word" << channelDataSize_ << ", ECON-D:eRx:channel=" << (int)econd << ":"
                                    << (int)erx << ":" << (int)channel << ", HGCalElectronicsId=" << id.raw()
                                    << "extracted channel data=0x" << std::hex << channelData_[channelDataSize_].raw();
            channelDataSize_++;
            iword++;
          }
        }
      }

      //----- parse the ECON-D trailer
      // (no information unpacked from ECON-D trailer, just skip it)
      iword += 1;  // length of an ECON-D trailer (32 bits CRC)

      if (iword - econdBodyStart != payloadLength)
        throw cms::Exception("CorruptData")
            << "Mismatch between unpacked and expected ECON-D #" << (int)econd << " payload length\n"
            << "  unpacked payload length=" << iword - econdBodyStart << "\n"
            << "  expected payload length=" << payloadLength;
      // pad to 2 words
      if (iword % 2 != 0) {
        LogDebug("HGCalUnpacker") << "Padding ECON-D payload to 2 32-bit words (remainder: " << (iword % 2) << ").";
        iword += 1;
      }
    }
    captureBlock++;  // the capture block has no trailer to parse
  }
  channelData_.resize(channelDataSize_);
  commonModeSum_.resize(channelDataSize_);
  commonModeData_.resize(commonModeDataSize_);
  return;
}

void HGCalUnpacker::parseECOND(
    const std::vector<uint32_t>& inputArray,
    const std::function<uint16_t(uint16_t sLink, uint8_t captureBlock, uint8_t econd)>& enabledERXMapping) {
  uint16_t sLink = 0;
  bool zside(false); //fixme this should be based on the slink id
  uint8_t captureBlock = 0;
  uint8_t econd = 0;

  channelDataSize_ = 0;
  commonModeDataSize_ = 0;
  flaggedECOND_.clear();

  for (uint32_t iword = 0; iword < inputArray.size();) {  // loop through all ECON-Ds
    //----- parse the ECON-D header
    // (the second word of ECON-D header contains no information for unpacking, use only the first one)
    if (((inputArray[iword] >> kHeaderShift) & kHeaderMask) != config_.econdHeaderMarker)  // sanity check
      throw cms::Exception("CorruptData")
          << "Expected a ECON-D header at word " << std::dec << iword << "/0x" << std::hex << iword << " (marker: 0x"
          << config_.econdHeaderMarker << "), got 0x" << inputArray[iword] << ".";

    const uint32_t econdHeader = inputArray[iword];
    iword += 2;  // length of ECON-D header (2 * 32 bits)

    LogDebug("HGCalUnpack") << "ECON-D #" << (int)econd << ", first word of ECON-D header=0x" << std::hex
                            << econdHeader;

    //----- extract the payload length
    const uint32_t payloadLength = (econdHeader >> kPayloadLengthShift) & kPayloadLengthMask;
    if (payloadLength > config_.payloadLengthMax)  // payload length too big
      throw cms::Exception("CorruptData")
        << "Unpacked payload length=" << payloadLength << " exceeds the maximal length=" << config_.payloadLengthMax;

    LogDebug("HGCalUnpack")  << "ECON-D #" << (int)econd << ", payload length = " << payloadLength;
    
    //Quality check
    uint32_t econdQuality(
                          (((econdHeader >> kHTShift) & kHTMask) >= 0b10)*HGCalFlaggedECONDInfo::HTBITS +
                          (((econdHeader >> kEBOShift) & kEBOMask) >= 0b10)*HGCalFlaggedECONDInfo::EBOBITS +
                          (((econdHeader >> kMatchShift) & kMatchMask) == 0)*HGCalFlaggedECONDInfo::MATCHBIT +
                          (((econdHeader >> kTruncatedShift) & kTruncatedMask) == 1)*HGCalFlaggedECONDInfo::TRUNCATED);      
    if (econdQuality) {
      LogDebug("HGCalUnpack") << "ECON-D failed quality check, HT=" << (econdHeader >> kHTShift & kHTMask)
                              << ", EBO=" << (econdHeader >> kEBOShift & kEBOMask)
                              << ", M=" << (econdHeader >> kMatchShift & kMatchMask)
                              << ", T=" << (econdHeader >> kTruncatedShift & kTruncatedMask);
      HGCalElectronicsId eleid(zside, sLink, captureBlock, econd, 0, 0);
      flaggedECOND_.emplace_back(HGCalFlaggedECONDInfo(iword - 2,econdQuality,eleid.raw()));
      iword += payloadLength;  // skip the current ECON-D (using the payload length parsed above)

      continue;  // go to the next ECON-D
    }

    //----- perse the ECON-D body
    const uint32_t econdBodyStart = iword;  // for the ECON-D length check
    if (((econdHeader >> kPassThroughShift) & kPassThroughMask) == 0) {
      // standard ECON-D
      LogDebug("HGCalUnpack") << "Standard ECON-D";
      const auto enabledERX = enabledERXMapping(sLink, captureBlock, econd);
      for (uint8_t erx = 0; erx < config_.econdERXMax; erx++) {  // loop through all eRxs
        if ((enabledERX >> erx & 1) == 0)
          continue;  // only pick active eRxs

        //----- parse the eRX subpacket header
        //common mode
        const HGCalElectronicsId cm0id(zside, sLink, captureBlock, econd, erx, 37);
        const HGCalElectronicsId cm1id(zside, sLink, captureBlock, econd, erx, 38);
        LogDebug("HGCalUnpack") << "ECON-D:eRx=" << (int)econd << ":" << (int)erx
                                << ", first word of the eRx header=0x" << std::hex << inputArray[iword] << "\n"
                                << "  extracted common mode 0 HGCalElectronicsId=" << std::dec<<cm0id.raw() << ",adc=0x" << std::hex
                                << ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) << "\n"
                                << "  extracted common mode 1 HGCalElectronicsId=" << std::dec<<cm1id.raw() << ",adc=0x" << std::hex
                                << ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask) << "\n";
        //assume common mode in char mode
        commonModeData_[commonModeDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
            cm0id,
            ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask)<<10);
        commonModeData_[commonModeDataSize_ + 1] = HGCROCChannelDataFrame<HGCalElectronicsId>(
            cm1id,
            ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask)<<10);
        uint16_t cmSum = ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) + ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask);
        if (commonModeDataSize_ + 1 > config_.commonModeMax)
          throw cms::Exception("HGCalUnpack") << "Too many common mode data unpacked: " << (commonModeDataSize_ + 1)
                                              << " >= " << config_.commonModeMax << ".";
        commonModeDataSize_ += 2;

        if (((inputArray[iword] >> kFormatShift) & kFormatMask) == 1) {  // empty eRx
          LogDebug("HGCalUnpack") << "eRx empty";
          iword += 1;  // length of an empty eRx header (32 bits)

          continue;  // skip to the next eRx
        }

        // regular mode
        const uint64_t erxHeader = ((uint64_t)inputArray[iword] << 32) | ((uint64_t)inputArray[iword + 1]);
        iword += 2;  // length of a standard eRx header (2 * 32 bits)
        LogDebug("HGCalUnpack") << "whole eRx header=0x" << std::hex << erxHeader;

        //----- parse eRx subpacket body
        uint32_t bitCounter = 0;
        for (uint8_t channel = 0; channel < config_.erxChannelMax; channel++) {  // loop through all channels in eRx
          if (((erxHeader >> channel) & 1) == 0)
            continue;  // only pick active channels
          const HGCalElectronicsId id(zside, sLink, captureBlock, econd, erx, channel);
          const uint32_t tempIndex = bitCounter / 32 + iword;
          const uint8_t tempBit = bitCounter % 32;
          const uint32_t temp =
              (tempBit == 0) ? inputArray[tempIndex]
                             : (inputArray[tempIndex] << tempBit) | (inputArray[tempIndex + 1] >> (32 - tempBit));
          const uint8_t code = temp >> 28;
          // use if and else here
          channelData_[channelDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
              id, 
              ((temp << erxBodyLeftShift_[code]) >> erxBodyRightShift_[code]) & erxBodyMask_[code]);
          bitCounter += erxBodyBits_[code];
          if (code == 0b0010)
            channelData_[channelDataSize_].fillFlag1(1);
          commonModeSum_[channelDataSize_]=cmSum;
          LogDebug("HGCalUnpack") << "Word " << channelDataSize_ << ", ECON-D:eRx:channel=" << (int)econd << ":"
                                  << (int)erx << ":" << (int)channel << "\n"
                                  << "  full word readout=0x" << std::hex << temp << std::dec << ", code=0x" << std::hex
                                  << (int)code << std::dec << "\n"
                                  << "  extracted channel data=0x" << std::hex << channelData_[channelDataSize_].raw();
          channelDataSize_++;
        }
        // pad to the whole word
        iword += bitCounter / 32;
        if (bitCounter % 32 != 0)
          iword += 1;
        // eRx subpacket has no trailer
      }
    } else {
      // passthrough ECON-D
      LogDebug("HGCalUnpack") << "Passthrough ECON-D";
      const auto enabledERX = enabledERXMapping(sLink, captureBlock, econd);
      for (uint8_t erx = 0; erx < config_.econdERXMax; erx++) {  // loop through all eRxs
        if ((enabledERX >> erx & 1) == 0)
          continue;  // only pick active eRx
        //----- parse the eRX subpacket header
        //common mode
        const HGCalElectronicsId cm0id(zside, sLink, captureBlock, econd, erx, 37);
        const HGCalElectronicsId cm1id(zside, sLink, captureBlock, econd, erx, 38);
        LogDebug("HGCalUnpack") << "ECON-D:eRx=" << (int)econd << ":" << (int)erx
                                << ", first word of the eRx header=0x" << std::hex << inputArray[iword] << "\n"
                                << "  extracted common mode 0 HGCalElectronicsId=" << std::dec<<cm0id.raw() << ",adc=0x" << std::hex
                                << ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) << "\n"
                                << "  extracted common mode 1 HGCalElectronicsId=" << std::dec<<cm1id.raw() << ",adc=0x" << std::hex
                                << ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask) << "\n";
        //assume common mode in char mode
        commonModeData_[commonModeDataSize_] = HGCROCChannelDataFrame<HGCalElectronicsId>(
            cm0id,
            ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask)<<10);
        commonModeData_[commonModeDataSize_ + 1] = HGCROCChannelDataFrame<HGCalElectronicsId>(
            cm1id,
            ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask)<<10);
        uint16_t cmSum = ((inputArray[iword] >> kCommonmode0Shift) & kCommonmode0Mask) + ((inputArray[iword] >> kCommonmode1Shift) & kCommonmode1Mask);
        iword += 2;  // length of the standard eRx header (2 * 32 bits)

        for (uint8_t channel = 0; channel < config_.erxChannelMax; channel++) {  // loop through all channels in eRx
          const HGCalElectronicsId id(zside, sLink, captureBlock, econd, erx, channel);
          channelData_[channelDataSize_] =
              HGCROCChannelDataFrame<HGCalElectronicsId>(id, inputArray[iword]);
          commonModeSum_[channelDataSize_]=cmSum;
          LogDebug("HGCalUnpack") << "Word " << channelDataSize_ << ", ECON-D:eRx:channel=" << (int)econd << ":"
                                  << (int)erx << ":" << (int)channel << ", HGCalElectronicsId=" << id.raw() << "\n"
                                  << "extracted channel data=0x" << std::hex << channelData_.at(channelDataSize_).raw();
          channelDataSize_++;
          iword++;
        }
        if (commonModeDataSize_ + 1 > config_.commonModeMax)
          throw cms::Exception("HGCalUnpack") << "Too many common mode data unpacked: " << (commonModeDataSize_ + 1)
                                              << " >= " << config_.commonModeMax << ".";
        commonModeDataSize_ += 2;
      }
    }
    //----- fill the ECON-D trailer
    // (no information is needed from ECON-D trailer in unpacker, skip it)
    iword += 1;  // length of the ECON-D trailer (32 bits CRC)

    if (iword - econdBodyStart != payloadLength)
      throw cms::Exception("CorruptData")
          << "Mismatch between unpacked and expected ECON-D #" << (int)econd << " payload length\n"
          << "  unpacked payload length=" << iword - econdBodyStart << "\n"
          << "  expected payload length=" << payloadLength;
  }
  channelData_.resize(channelDataSize_);
  commonModeSum_.resize(channelDataSize_);
  commonModeData_.resize(commonModeDataSize_);
  return;
}
