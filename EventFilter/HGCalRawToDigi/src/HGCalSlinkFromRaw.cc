#include "EventFilter/HGCalRawToDigi/interface/HGCalSlinkFromRaw.h"
#include <filesystem>

// example reader by P.Dauncey, using https://gitlab.cern.ch/pdauncey/hgcal10glinkreceiver

using namespace hgcal;

SlinkFromRaw::SlinkFromRaw(const edm::ParameterSet &iConfig)
    : SlinkEmulatorBase(iConfig), fedIds_(iConfig.getUntrackedParameter<std::vector<unsigned>>("fedIds")) {
  auto inputfiles = iConfig.getUntrackedParameter<std::vector<std::string>>("inputs");
  if (inputfiles.size() % fedIds_.size() != 0) {
    throw cms::Exception("[HGCalSlinkFromRaw::SlinkFromRaw]")
        << "Number of inputs (" << inputfiles.size() << ") cannot be devided by the number of fedIds ("
        << fedIds_.size() << ")";
  }

  std::vector<std::vector<std::string>> fileLists(fedIds_.size());
  for (unsigned i = 0; i < inputfiles.size(); ++i) {
    if (std::filesystem::exists(inputfiles[i])) {
      fileLists.at(i % fedIds_.size()).push_back(inputfiles[i]);
    }
  }

  for (unsigned idx = 0; idx < fedIds_.size(); ++idx) {
    readers_[fedIds_[idx]] = std::make_shared<SlinkFileReader>(fileLists[idx], fedIds_[idx]);
  }

  auto trig_inputs = iConfig.getUntrackedParameter<std::vector<std::string>>("trig_inputs", {});
  readers_[SlinkFileReader::kTrigIdOffset] =
      std::make_shared<SlinkFileReader>(trig_inputs, SlinkFileReader::kTrigIdOffset);
}

std::unique_ptr<FEDRawDataCollection> SlinkFromRaw::next() {
  auto raw_data = std::make_unique<FEDRawDataCollection>();
  auto copyToFEDRawData = [&raw_data](const hgcal_slinkfromraw::RecordRunning *rEvent, unsigned fedId) {
    //rEvent->payloadLength() - 1: last word is a 0xdeadbeefdeadbeef which can be disregarded
    size_t total_event_size = (rEvent->payloadLength() - 1) * sizeof(uint64_t) / sizeof(char);
    auto &fed_data = raw_data->FEDData(fedId);
    fed_data.resize(total_event_size);
    memcpy(fed_data.data(), (const char *)rEvent->payload(), total_event_size);
  };

  // read DAQ Slinks
  for (unsigned idx = 0; idx < fedIds_.size(); ++idx) {
    const auto &fedId = fedIds_[idx];
    auto reader = readers_.at(fedId);
    auto rEvent = reader->nextEvent();
    if (!rEvent) {
      throw cms::Exception("[SlinkFromRaw::next]") << "record is null for fedId=" << fedId;
    }

    if (idx == 0) {
      eventId_ = rEvent->slinkBoe()->eventId();
      bxId_ = rEvent->slinkEoe()->bxId();
      orbitId_ = rEvent->slinkEoe()->orbitId();
      copyToFEDRawData(rEvent, fedId);
    } else {
      // find the event matched to the first slink
      while (rEvent) {
        if (rEvent->slinkBoe()->eventId() < eventId_) {
          rEvent = reader->nextEvent();
          continue;
        }
        if (rEvent->slinkBoe()->eventId() == eventId_ && rEvent->slinkEoe()->bxId() == bxId_ &&
            rEvent->slinkEoe()->orbitId() == orbitId_) {
          copyToFEDRawData(rEvent, fedId);
          break;
        }
      }
    }
  }

  // read trigger Slink
  metaData_ = HGCalTestSystemMetaData();
  {
    auto reader = readers_.at(SlinkFileReader::kTrigIdOffset);

    // rTrgEvent will be null if there are no trig_inputs
    auto rTrgEvent = reader->nextEvent();

    while (rTrgEvent) {
      // find the trigger event matched to the first DAQ slink
      if (rTrgEvent->slinkBoe()->eventId() < eventId_) {
        rTrgEvent = reader->nextEvent();
        continue;
      }
      if (rTrgEvent->slinkBoe()->eventId() == eventId_ && rTrgEvent->slinkEoe()->bxId() == bxId_ &&
          rTrgEvent->slinkEoe()->orbitId() == orbitId_) {
        metaData_.trigType_ = rTrgEvent->slinkBoe()->l1aType();
        readTriggerData(rTrgEvent);
        break;
      }
    }
  }

  return raw_data;
}

void SlinkFromRaw::readTriggerData(const hgcal_slinkfromraw::RecordRunning *rTrgEvent) {
  constexpr uint64_t pkt_mask = 0xff;
  constexpr uint64_t pkt_sep = 0xfecafecafecafe;

  // TODO: use implementations from std <bit> in c++20
  constexpr auto countl_zero = [](uint32_t input) -> unsigned char {
    if (input == 0) {
      return 32;
    }
    constexpr uint32_t highestBit = 1 << 31;
    unsigned char result = 0;
    for (; (input & highestBit) == 0; input <<= 1) {
      ++result;
    }
    return result;
  };

  constexpr auto countr_zero = [](uint32_t input) -> unsigned char {
    if (input == 0) {
      return 32;
    }
    unsigned char result = 0;
    for (; (input & 1) == 0; input >>= 1) {
      ++result;
    }
    return result;
  };

  if (rTrgEvent && rTrgEvent->payloadLength() > 0) {
    auto p = (const uint64_t *)rTrgEvent;
    int length = 0;
    p += 4;  // (1 record header + 2 slink header + 1 trigger readout header)
    for (unsigned iblock = 0; iblock < 4 && p < (const uint64_t *)rTrgEvent + rTrgEvent->payloadLength(); ++iblock) {
      LogDebug("SlinkFromRaw") << "Header: " << std::hex << std::setfill('0') << "0x" << *p << std::endl;
      assert((*p >> 8) == pkt_sep);
      length = *p & pkt_mask;
      if (iblock == 2) {
        // scintillator
        // the length should be 9 (BX) * 5 (64b word)
        // only the 1st 64b word is used; the last (5th) word is a separator
        auto p_scint = p + 1;
        uint32_t trigtime = 0;
        uint32_t trigwidth = 0;
        bool triggered = false;
        while (p_scint <= p + length) {
          // Bits [31:  0] : External Trigger
          // Bits [63: 32] : 0xABCDFEED
          assert((*p_scint >> 32) == 0xABCDFEED);
          uint32_t trigbits = *p_scint & 0xFFFFFFFF;
          LogDebug("SlinkFromRaw") << "BX " << (p_scint - p) / 5 << ": " << std::hex << std::setfill('0') << "0x"
                                   << *p_scint << ", trigbits = "
                                   << "0x" << trigbits << std::endl;
          if (not triggered) {
            trigtime += countl_zero(trigbits);
            if (trigbits > 0) {
              // first BX with the trigger fired
              triggered = true;
              // count the 1s from the right
              trigwidth += countr_zero(~trigbits);
            }
          } else {
            // trigger already fired in previous BX
            if (trigbits > 0) {
              // trigger signal extends more than 1 BX
              // count the 1s from the left
              trigwidth += countl_zero(~trigbits);
            } else if (trigbits == 0) {
              // stop processing when the trigger is no longer fired
              break;
            }
          }
          p_scint += 5;
        }
        LogDebug("SlinkFromRaw") << "==> trigtime = " << std::dec << std::setfill(' ') << trigtime
                                 << ", trigwidth = " << trigwidth << std::endl;
        metaData_.trigTime_ = trigtime;
        metaData_.trigWidth_ = trigwidth;
        break;
      }
      p += length + 1;
    }
  }
}
