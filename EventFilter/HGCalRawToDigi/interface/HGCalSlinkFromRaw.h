#ifndef EventFilter_HGCalRawToDigi_HGCalSlinkFromRaw_h
#define EventFilter_HGCalRawToDigi_HGCalSlinkFromRaw_h

#include "EventFilter/HGCalRawToDigi/interface/HGCalRawDataBaseEmulator.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalSlinkFromRaw/FileReader.h"

#include <ostream>
#include <iostream>
#include <fstream>

namespace hgcal {

  class SlinkFileReader {
  public:
    SlinkFileReader(const std::vector<std::string> &filelist, unsigned fedId)
        : inputfiles_(filelist), fedId_(fedId), record_(new hgcal_slinkfromraw::RecordT<4095>) {
      std::cout << "[SlinkFileReader] fedId=" << fedId_ << ", files:\n";
      copy(begin(inputfiles_), end(inputfiles_), std::ostream_iterator<std::string>{std::cout, "\n"});
    }

    const hgcal_slinkfromraw::RecordRunning *nextEvent() {
      if (inputfiles_.empty())
        return nullptr;

      // open a new file
      if (fileReader_.closed()) {
        fileReader_.open(inputfiles_[ifile_]);
      }

      //no more records in the file, move to next
      if (!fileReader_.read(record_)) {
        fileReader_.close();

        ifile_++;
        if (ifile_ >= inputfiles_.size())
          throw cms::Exception("SlinkFileReader") << "no more files";

        return nextEvent();
      }

      //if record is stop or starting read again
      if (record_->state() == hgcal_slinkfromraw::FsmState::Stopping) {
        edm::LogInfo("SlinkFromRaw") << "RecordStopping will search for next";
        const hgcal_slinkfromraw::RecordStopping *rStop((hgcal_slinkfromraw::RecordStopping *)record_);
        std::cout << "[fedId=" << fedId_ << "]\n";
        rStop->print();
        return nextEvent();
      }
      if (record_->state() == hgcal_slinkfromraw::FsmState::Starting) {
        edm::LogInfo("SlinkFromRaw") << "RecordStarting will search for next";
        const hgcal_slinkfromraw::RecordStarting *rStart((hgcal_slinkfromraw::RecordStarting *)record_);
        std::cout << "[fedId=" << fedId_ << "]\n";
        rStart->print();
        return nextEvent();
      }
      if (record_->state() == hgcal_slinkfromraw::FsmState::Continuing) {
        edm::LogInfo("SlinkFromRaw") << "RecordContinuing";
        const hgcal_slinkfromraw::RecordContinuing *rCont((hgcal_slinkfromraw::RecordContinuing *)record_);
        std::cout << "[fedId=" << fedId_ << "]\n";
        rCont->print();
        return nextEvent();
      }

      const auto *rEvent = (hgcal_slinkfromraw::RecordRunning *)record_;
      if (!rEvent->valid())
        throw cms::Exception("[SlinkFileReader::nextEvent]") << "record running is invalid for fedId=" << fedId_;

      if (firstEvent_) {
        std::cout << "[fedId=" << fedId_ << "]\n";
        rEvent->print();
        firstEvent_ = false;
      }

      return rEvent;
    }

    static constexpr unsigned kTrigIdOffset = 10000;

  private:
    const std::vector<std::string> inputfiles_;
    const unsigned fedId_;

    hgcal_slinkfromraw::FileReader fileReader_;
    hgcal_slinkfromraw::RecordT<4095> *record_;

    bool firstEvent_ = true;
    unsigned ifile_ = 0;
  };

  /// a reader of s-link data from binary files
  class SlinkFromRaw : public SlinkEmulatorBase {
  public:
    /// \param[in] configuration parameters
    explicit SlinkFromRaw(const edm::ParameterSet &iConfig);

    std::unique_ptr<FEDRawDataCollection> next() override;
    HGCalTestSystemMetaData nextMetaData() override { return metaData_; }

  private:
    void readTriggerData(const hgcal_slinkfromraw::RecordRunning *);

    HGCalTestSystemMetaData metaData_;

    std::vector<unsigned> fedIds_;
    std::map<unsigned, std::shared_ptr<SlinkFileReader>> readers_;

    uint64_t eventId_ = 0;
    uint16_t bxId_ = 0;
    uint32_t orbitId_ = 0;
  };

}  // namespace hgcal

#endif
