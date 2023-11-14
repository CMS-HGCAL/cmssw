#include <iomanip> // for std::setw
#include <future>
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/EDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"
#include "DataFormats/PortableTestObjects/interface/alpaka/TestDeviceCollection.h"

#include "CondFormats/DataRecord/interface/HGCalMappingModuleIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingModuleRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiCellIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiPMCellIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHostCollection.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDeviceCollection.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalMappingESSourceTester : public stream::EDProducer<> {
  public:
    explicit HGCalMappingESSourceTester(const edm::ParameterSet&);
    static void fillDescriptions(edm::ConfigurationDescriptions&);

  private:

    void produce(device::Event&, device::EventSetup const&) override;
    void beginRun(edm::Run const&, edm::EventSetup const&) override;

    edm::ESWatcher<HGCalMappingModuleIndexerRcd> cfgWatcher_;
    edm::ESGetToken<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd> moduleIndexTkn_;
    edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd> siIndexTkn_;
    edm::ESGetToken<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd> sipmIndexTkn_;
    device::ESGetToken<hgcal::HGCalMappingModuleParamDeviceCollection, HGCalMappingModuleRcd> moduleTkn_;

    const device::EDPutToken<portabletest::TestDeviceCollection> testCollToken_;
  };

  //
  HGCalMappingESSourceTester::HGCalMappingESSourceTester(const edm::ParameterSet& iConfig)
    : moduleIndexTkn_(esConsumes<HGCalMappingModuleIndexer,HGCalMappingModuleIndexerRcd>()),
      siIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingSiCellIndexerRcd>()),
      sipmIndexTkn_(esConsumes<HGCalMappingCellIndexer,HGCalMappingSiPMCellIndexerRcd>()),
      moduleTkn_(esConsumes(edm::ESInputTag(""))),
      testCollToken_{produces()} {
  }

  //
  void HGCalMappingESSourceTester::beginRun(edm::Run const& iRun, edm::EventSetup const& iSetup){
  }

  void HGCalMappingESSourceTester::produce(device::Event& iEvent, device::EventSetup const& iSetup) {

    //put a dummy collection to the event
    portabletest::TestDeviceCollection testColl{0,iEvent.queue()};
    iEvent.emplace(testCollToken_, std::move(testColl));
    
    // if the cfg didn't change there's nothing else to do
    if (!cfgWatcher_.check(iSetup)) return;

    //get indexers
    auto modulesIdx = iSetup.getData(moduleIndexTkn_);
    auto siIdx = iSetup.getData(siIndexTkn_);
    auto sipmIdx = iSetup.getData(sipmIndexTkn_);
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "Dense indexers retrieved for HGCAL";
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Module indexer]"
                                                    << "\n\t max FED=" << modulesIdx.idxParams_.maxFEDsPerEndcap
                                                    <<" max CB/FED=" << modulesIdx.idxParams_.sLinkCaptureBlockMax
                                                    <<" max ECON/CB=" << modulesIdx.idxParams_.captureBlockECONDMax
                                                    <<" max eRx/ECON=" << modulesIdx.idxParams_.econdERXMax
                                                    <<" max ch/eRx=" << modulesIdx.idxParams_.erxChannelMax      
                                                    << "\n\t Total size is=" << modulesIdx.getSize()
                                                    << " size at ROC is=" << modulesIdx.getSize(true);
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[Si cell indexer]"
                                                    << "\n\t max types=" << siIdx.idxParams_.moduleTypeMax
                                                    << "\t max ROC / type=" << siIdx.idxParams_.cellChipMax
                                                    << "\t max half / ROC =" << siIdx.idxParams_.halfROCMax
                                                    << "\t max ch / half=" << siIdx.idxParams_.channelSeqMax
                                                    << "\n\t Total size is=" << siIdx.getSize();
    edm::LogInfo("HGCalMappingIndexESSourceTester") << "[SiPM-on-tile cell indexer]"
                                                    << "\n\t max types=" << sipmIdx.idxParams_.moduleTypeMax
                                                    << "\t max ROC / type=" << sipmIdx.idxParams_.cellChipMax
                                                    << "\t max half / ROC =" << sipmIdx.idxParams_.halfROCMax
                                                    << "\t max ch / half=" << sipmIdx.idxParams_.channelSeqMax
                                                    << "\n\t Total size is=" << sipmIdx.getSize();
    
    const auto modules = iSetup.getData(moduleTkn_);
    //for(int i=0; i<deviceCalibParamProvider.view().metadata().size(); i++) {
    //      LogDebug("HGCalCalibrationParameter")
    //          << "idx = "         << i << ", "
    //          << "pedestal = "    << deviceCalibParamProvider.view()[i].pedestal()   << ", "
    //          << "CM_slope = "    << deviceCalibParamProvider.view()[i].CM_slope()   << ", "
    //          << "CM_offset = "   << deviceCalibParamProvider.view()[i].CM_offset()  << ", "
    //          << "BXm1_slope = "  << deviceCalibParamProvider.view()[i].BXm1_slope() << ", "
    //          << "BXm1_offset = " << deviceCalibParamProvider.view()[i].BXm1_offset(); //<< std::endl;
    //  }
  }

  //
  void HGCalMappingESSourceTester::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    descriptions.addWithDefaultLabel(desc);
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

// define this as a plug-in
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalMappingESSourceTester);
