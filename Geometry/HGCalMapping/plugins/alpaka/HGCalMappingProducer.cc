// CMSSW includes
// #include "DataFormats/HGCalDigi/interface/HGCalDigiHostCollection.h"
// #include "DataFormats/HGCalDigi/interface/alpaka/HGCalDigiDeviceCollection.h"
// #include "DataFormats/HGCalRecHit/interface/HGCalRecHitHostCollection.h"
// #include "DataFormats/HGCalRecHit/interface/alpaka/HGCalRecHitDeviceCollection.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/EDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"
// #include "RecoLocalCalo/HGCalRecAlgos/plugins/alpaka/HGCalRecHitCalibrationAlgorithms.h"
#include <iomanip> // for std::setw
#include <future>

// includes for size, calibration, and configuration parameters
#include "FWCore/Framework/interface/ESWatcher.h"
#include "Geometry/HGCalMapping/interface/HGCalMappingParameterHostCollection.h"
#include "Geometry/HGCalMapping/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

#include "CondFormats/DataRecord/interface/HGCalCondSerializableModuleInfoRcd.h"

template<class T> double duration(T t0,T t1)
{
  auto elapsed_secs = t1-t0;
  typedef std::chrono::duration<float> float_seconds;
  auto secs = std::chrono::duration_cast<float_seconds>(elapsed_secs);
  return secs.count();
}

inline std::chrono::time_point<std::chrono::steady_clock> now()
{
  return std::chrono::steady_clock::now();
}

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalMappingProducer : public stream::EDProducer<> {
  public:
    explicit HGCalMappingProducer(const edm::ParameterSet&);
    static void fillDescriptions(edm::ConfigurationDescriptions&);

  private:
    void produce(device::Event&, device::EventSetup const&) override;
    void beginRun(edm::Run const&, edm::EventSetup const&) override;

    edm::ESWatcher<HGCalCondSerializableModuleInfoRcd> moduleWatcher_;

    device::ESGetToken<hgcal::HGCalMappingModuleParamDeviceCollection, HGCalCondSerializableModuleInfoRcd> moduleToken_;

    //  HGCalRecHitCalibrationAlgorithms calibrator_;  // cannot be "const" because the calibrate() method is not const
  };

  HGCalMappingProducer::HGCalMappingProducer(const edm::ParameterSet& iConfig)
    // : calibrator_{HGCalRecHitCalibrationAlgorithms(
    //   iConfig.getParameter<int>("n_blocks"),
    //   iConfig.getParameter<int>("n_threads"))}
    {
      moduleToken_ = esConsumes(iConfig.getParameter<edm::ESInputTag>("moduleSource"));
    }

  void HGCalMappingProducer::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup){
  }

  void HGCalMappingProducer::produce(device::Event& iEvent, device::EventSetup const& iSetup) {
    auto queue = iEvent.queue();

    // Read digis
    auto const& deviceMappingParamProvider = iSetup.getData(moduleToken_);

    // Check if there are new conditions and read them
    // if (configWatcher_.check(iSetup)){
      for(int i=0; i<deviceMappingParamProvider.view().metadata().size(); i++) {
          LogDebug("HGCalMappingParameter")
              << "idx = "              << i << ", "
              << "zside = "            << deviceMappingParamProvider.view()[i].zside()           << ", "
              << "isSiPM = "           << deviceMappingParamProvider.view()[i].isSiPM()          << ", "
              << "isHD = "             << deviceMappingParamProvider.view()[i].isHD()            << ", "
              << "plane = "            << deviceMappingParamProvider.view()[i].plane()           << ", "
              << "u = "                << deviceMappingParamProvider.view()[i].u()               << ", "
              << "v = "                << deviceMappingParamProvider.view()[i].v()               << ", "
              << "fedid = "            << deviceMappingParamProvider.view()[i].fedid()           << ", "
              << "slink = "            << deviceMappingParamProvider.view()[i].slink()           << ", "
              << "wafType = "          << deviceMappingParamProvider.view()[i].wafType()         << ", "
              << "captureblock = "     << deviceMappingParamProvider.view()[i].captureblock()    << ", "
              << "econdidx = "         << deviceMappingParamProvider.view()[i].econdidx()        << ", "
              << "captureblockidx = "  << deviceMappingParamProvider.view()[i].captureblockidx() << std::endl;
      }
  }

  void HGCalMappingProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add("moduleSource", edm::ESInputTag{})->setComment("Label for module mapping parameters");
    // desc.add<int>("n_blocks", -1);
    // desc.add<int>("n_threads", -1);
    descriptions.addWithDefaultLabel(desc);
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

// define this as a plug-in
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalMappingProducer);