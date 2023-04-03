#ifndef EventFilter_HGCalRawToDigi_HGCalFrameGenerator_h
#define EventFilter_HGCalRawToDigi_HGCalFrameGenerator_h

#include "DataFormats/HGCalDigi/interface/HGCalRawDataEmulatorInfo.h"
#include "EventFilter/HGCalRawToDigi/interface/SlinkTypes.h"

#include <cstdint>
#include <vector>

namespace edm {
  class ParameterSet;
  class ParameterSetDescription;
}  // namespace edm
namespace CLHEP {
  class HepRandomEngine;
}

namespace hgcal {
  class HGCalFrameGenerator {
  public:
    explicit HGCalFrameGenerator(const edm::ParameterSet&);

    static edm::ParameterSetDescription description();

    void setRandomEngine(CLHEP::HepRandomEngine& rng);

    std::vector<uint32_t> produceECONEvent(const econd::ECONDEvent&) const;
    const HGCalECONDEmulatorInfo& lastECONDEmulatedInfo() const { return last_econd_emul_info_; }

    /// Produce a S-link event from an input emulated event
    std::vector<uint64_t> produceSlinkEvent(uint32_t fed_id, const econd::ECONDEvent&) const;
    const HGCalSlinkEmulatorInfo& lastSlinkEmulatedInfo() const { return last_slink_emul_info_; }

    struct ECONDParameters {
      double chan_surv_prob{1.};
      std::vector<unsigned int> enabled_channels{};
      unsigned int header_marker{0};
      unsigned int num_channels{0};
      double bitO_error_prob{0.}, bitB_error_prob{0.}, bitE_error_prob{0.}, bitT_error_prob{0.}, bitH_error_prob{0.},
          bitS_error_prob{0.};
    };
    const ECONDParameters& econdParams() const { return econd_; }

    struct SlinkParameters {
      unsigned int num_econds{0};  //FIXME possibly replaced by a list of ECON-D's
      unsigned int boe_marker{0}, eoe_marker{0}, format_version{0};
    };
    const SlinkParameters& slinkParams() const { return slink_; }

  private:
    std::vector<bool> generateEnabledChannels() const;
    std::vector<uint32_t> generateERxData(const econd::ERxEvent&) const;

    static constexpr size_t max_num_econds_ = 12;
    const bool pass_through_;
    const bool expected_mode_;
    const bool matching_ebo_numbers_;
    const bool bo_truncated_;

    struct HeaderBits {
      bool bitO, bitB, bitE, bitT, bitH, bitS;
    };
    HeaderBits generateStatusBits() const;
    /// 8bit CRC for event header
    uint8_t computeCRC(const std::vector<uint32_t>&) const;

    ECONDParameters econd_;
    SlinkParameters slink_;
    CLHEP::HepRandomEngine* rng_{nullptr};  // NOT owning

    mutable HGCalECONDEmulatorInfo last_econd_emul_info_;
    mutable HGCalSlinkEmulatorInfo last_slink_emul_info_;
  };
}  // namespace hgcal

#endif
