#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cmath>

// framework imports
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/StreamID.h"

// alpaka imports
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/global/EDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToDevice.h"

// data formats
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"
#include "DataFormats/HGCalDigi/interface/HGCalSimHitAccumHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDevice.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "DataFormats/DetId/interface/DetId.h"



namespace ALPAKA_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;

  class HGCalSimHitAccumulator : public global::EDProducer<> {
  public:
    explicit HGCalSimHitAccumulator(const edm::ParameterSet&);

    /**
    @short declares configuration parameters allowed
    */
    static void fillDescriptions(edm::ConfigurationDescriptions&);

    /**
    @short the actual producing method
    */
    void produce(edm::StreamID , device::Event&, device::EventSetup const&) const override;

  private:

    /**
    @short reads the electronics dense indexing and builds an internal DetId -> DenseIndex mapping
    The mapping is only built once in a run. The `ncellsMapped_` variable is used to track if the map is available or not
    */
    int32_t buildGeoToDenseIndexMap(const hgcal::HGCalDenseIndexInfoDevice &, const hgcal::HGCalMappingCellParamHost&) const;
    
    const device::ESGetToken<hgcal::HGCalDenseIndexInfoDevice, HGCalDenseIndexInfoRcd> indexingToken_;
    const edm::ESGetToken<hgcal::HGCalMappingCellParamHost, HGCalElectronicsMappingRcd> cellMappingToken_;
    std::unordered_map<int32_t, edm::EDGetTokenT<edm::PCaloHitContainer> > g4HitsTokens_;
    std::unordered_map<int32_t, float> g4TOFdelay_;
    const edm::EDPutTokenT<hgcaldigi::HGCalSimHitAccumHost> g4AccumulatorToken_;
    mutable std::once_flag mapInitFlag_;
    mutable std::unordered_map<int32_t, std::unordered_map<uint32_t, int32_t>> det2dense_;
    mutable int32_t ncellsMapped_ = 0;
    static constexpr float nsPerBx_ = 25.0f;
  };

  //
  HGCalSimHitAccumulator::HGCalSimHitAccumulator(const edm::ParameterSet& iConfig)
      : EDProducer<>(iConfig),
        indexingToken_{esConsumes<>(iConfig.getParameter<edm::ESInputTag>("indexingSource"))},
        cellMappingToken_{esConsumes<>(iConfig.getParameter<edm::ESInputTag>("cellMappingSource"))},
        g4HitsTokens_{},
        g4AccumulatorToken_{produces()},
        ncellsMapped_(0) {

    //configure Geant4 hit sources to handle
    auto g4HitSubDet = iConfig.getParameter<std::vector<int32_t>>("g4HitsDets");
    auto g4HitInputs = iConfig.getParameter<std::vector<edm::InputTag>>("g4Hits");
    auto g4TOFDelays = iConfig.getParameter<std::vector<double>>("g4TOFDelays");
    if(g4HitSubDet.size()!=g4HitInputs.size() || g4TOFDelays.size()!=g4HitInputs.size())
      throw cms::Exception("HGCalSimHitAccumulator: Geant4 hit sub-detector codes, sources and time-of-flight delays are not fully consistent in size!");
    for (size_t i=0; i<g4HitSubDet.size(); i++) {
      auto det = g4HitSubDet[i];
      g4HitsTokens_[det] = consumes<edm::PCaloHitContainer>(g4HitInputs[i]);
      g4TOFdelay_[det] = static_cast<float>(g4TOFDelays[i]);
      edm::LogWarning("HGCalSimHitAccumulator::CTOR") << "will consume " << g4HitInputs[i] 
        << " PCaloHits collection for det=" << det
        << " TOF delay=" << g4TOFDelays[i];
    }

  }

  //
  void HGCalSimHitAccumulator::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add< std::vector<int32_t> >("g4HitsDets")->setComment("DetId detector codes expected for the sources of PCaloHits (see https://github.com/cms-sw/cmssw/blob/master/DataFormats/DetId/interface/DetId.h#L24)");
    desc.add< std::vector<edm::InputTag> >("g4Hits")->setComment("Label for source of PCaloHits (Geant4)");
    desc.add< std::vector<double> >("g4TOFDelays")->setComment("TOF delays to apply to PCaloHits (Geant4)");
    desc.add<edm::ESInputTag>("indexingSource")->setComment("Label for cell dense indexer");
    desc.add<edm::ESInputTag>("cellMappingSource")->setComment("Label for cell information type information");
    descriptions.addWithDefaultLabel(desc);
  }

  //
  int32_t HGCalSimHitAccumulator::buildGeoToDenseIndexMap(const hgcal::HGCalDenseIndexInfoDevice &denseIndexInfo, const hgcal::HGCalMappingCellParamHost &cellInfo) const {

    //loop over the indices and map the DetId for the physical cells
    auto const& cellInfo_view = cellInfo.const_view();
    auto const& denseIndexInfo_view = denseIndexInfo.const_view();
    int32_t ndenseIndices = denseIndexInfo_view.metadata().size();
    for(int32_t i=0; i<ndenseIndices; i++) {
      auto row = denseIndexInfo.view()[i];
      
      //cell type: if it's an unconnected channel just skip it
      auto cell_info_idx = row.cellInfoIdx();
      auto chType = cellInfo_view[cell_info_idx].t();
      if(chType<0) continue;

      const auto k = row.detid();
      const auto det = DetId(k).det();
      det2dense_[det][k] = i;
    }

    //sum up final mapped cells (and print some debug if needed)
    int32_t nmapped = 0;
    for(auto it : det2dense_) {
      int32_t ncells = static_cast<int32_t>(it.second.size());
      edm::LogWarning("HGCalSimHitAccumulator::buildGeoToDenseIndexMap") << "Detector " << it.first << " has " << ncells << " cells mapped";
      nmapped += ncells;
    }
    edm::LogWarning("HGCalSimHitAccumulator::buildGeoToDenseIndexMap") << "Total cells mapped:" << nmapped << " / " << ndenseIndices;

    return nmapped;
  }

  //
  void HGCalSimHitAccumulator::produce(edm::StreamID sid, device::Event& iEvent, device::EventSetup const& iSetup) const {
    
    //this is executed only once even if there are concurrent threads
    std::call_once(mapInitFlag_, [&] {
      auto const& denseIndexInfo = iSetup.getData(indexingToken_);
      auto const& cellInfo = iSetup.getData(cellMappingToken_);
      ncellsMapped_ = buildGeoToDenseIndexMap(denseIndexInfo, cellInfo);
    });

    //declare collection to produce
    hgcaldigi::HGCalSimHitAccumHost g4Accumulators(iEvent.queue(), ncellsMapped_);

    std::size_t totalHits = 0;
    for (auto const& g4It : g4HitsTokens_) {
      const auto k = g4It.first;
      const auto token = g4It.second;
      const auto& hit_collection = iEvent.get(token);

      edm::LogWarning("[HGCalSimHitAccumulator::acquire]") << "Det=" << k << " has " << hit_collection.size() << " hits" << std::endl;

      for (size_t i = 0; i < hit_collection.size(); ++i) {

        //skip hits which are not to be tracked
        const auto& hit = hit_collection.at(i);
        uint32_t did = static_cast<uint32_t>(hit.id());
        const auto det = DetId(did).det();
        auto idx = det2dense_[det].find(did);

        if(idx==det2dense_[det].end()) continue;

        float energy = hit.energy();
        float tof = hit.time() + g4TOFdelay_.at(det);
        int32_t bx = static_cast<int32_t>( std::roundl(tof/nsPerBx_) );
        
        std::cout << energy << " " << tof << " " << bx << std::endl;

      }
    
      totalHits += hit_collection.size();
    }
    edm::LogWarning("[HGCalSimHitAccumulator::acquire]") << "Have " << totalHits << " hits";
 
 
    edm::LogWarning("HGCalSimHitAccumulator") << "Storing accumulators for " << ncellsMapped_ << " in the event";
    iEvent.emplace(g4AccumulatorToken_, std::move(g4Accumulators));
  }

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#include "DataFormats/HGCalDigi/interface/alpaka/HGCalSimHitAccumDevice.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalSimHitAccumulator);