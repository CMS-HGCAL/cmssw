/****************************************************************************
 *
 * This is a part of HGCAL offline software.
 * Authors:
 *   Pedro Silva, CERN
 *   Laurent Forthomme, CERN
 *
 ****************************************************************************/

#ifndef EventFilter_HGCalRawToDigi_HGCalModuleTreeReader_h
#define EventFilter_HGCalRawToDigi_HGCalModuleTreeReader_h

#include "EventFilter/HGCalRawToDigi/interface/HGCalRawDataBaseEmulator.h"

namespace hgcal::econd {
  /// Read out a the relevant raw data produced by a module to memory and returns ECON-D frames on request
  /// \note The format is as agreed with system tests convenors so that it can be used in integration/beam tests
  class HGCalModuleTreeReader : public ECONDEmulatorBase {
  public:
    /// \param[in] tree_name Name of the TB events tree
    /// \param[in] filenames List of filenames to loop on
    /// \param[in] num_channels Channels multiplicity
    explicit HGCalModuleTreeReader(const EmulatorParameters&,
                                   const std::string& tree_name,
                                   const std::vector<std::string>& filenames);

    /// Input tree collections
    struct HGCModuleTreeEvent {
      unsigned int event, chip;
      int half, bxcounter, eventcounter, orbitcounter, trigtime, trigwidth;
      std::vector<unsigned int>* daqdata{nullptr};
    };
    std::unique_ptr<ECONDInput> next() override;

    HGCalTestSystemMetaData nextMetaData() override;
    
  private:
    
    uint32_t iEvent_,totalEvents_;    
    std::map< ERxId_t, std::vector<std::tuple<EventId, ERxData>> > data_;
    std::map< ERxId_t, std::vector<std::tuple<EventId, HGCalTestSystemMetaData>> > metadata_;    
  };

}  // namespace hgcal::econd

#endif
