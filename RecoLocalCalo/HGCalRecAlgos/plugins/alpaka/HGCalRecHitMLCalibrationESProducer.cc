#include "DataFormats/PortableTestObjects/interface/TestSoA.h"
#include "DataFormats/PortableTestObjects/interface/alpaka/ParticleDeviceCollection.h"
#include "DataFormats/PortableTestObjects/interface/alpaka/SimpleNetDeviceCollection.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDPutToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EventSetup.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
// #include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/FixedQueueEDProducer.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "PhysicsTools/PyTorchAlpaka/interface/TensorCollection.h"
#include "PhysicsTools/PyTorchAlpaka/interface/alpaka/AlpakaModel.h"
#include "PhysicsTools/PyTorchAlpakaTest/interface/Environment.h"

#include "DataFormats/HGCalDigi/interface/HGCalECONDPacketInfoHost.h"
#include "DataFormats/HGCalDigi/interface/HGCalECONDPacketInfoSoA.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiHost.h"

namespace ALPAKA_ACCELERATOR_NAMESPACE::torchtest {

  class CMMLCalibration : public stream::ESProducer<> {
  public:
    CMMLCalibration(const edm::ParameterSet &params)
        : ESProducer<>(params),
          econdInfoTkn_(consumes<hgcaldigi::HGCalECONDPacketInfoHost>(iConfig.getParameter<edm::InputTag>("ECONDPacketInfo"))),
          digisTkn_(consumes<hgcaldigi::HGCalDigiHost>(iConfig.getParameter<edm::InputTag>("hgcalDigis"))),
          simple_net_token_{produces()},
          model_(params.getParameter<edm::FileInPath>("model").fullPath()),
          environment_{static_cast<::torchtest::Environment>(params.getUntrackedParameter<int>("environment"))} {}

    static void fillDescriptions(edm::ConfigurationDescriptions &descriptions) {
      edm::ParameterSetDescription desc;
      desc.add<edm::FileInPath>("model");
      desc.add<edm::InputTag>("ECONDPacketInfo");
      desc.add<edm::InputTag>("hgcalDigis");
      desc.addUntracked<int>("environment", static_cast<int>(::torchtest::Environment::kProduction));
      descriptions.addWithDefaultLabel(desc);
    }

    void getDataStructures(device::Event &event, const edm::EventSetup &event_setup) {
      const auto econdInfo = event.get(econdInfoTkn_)->const_view();
      // Iterate through each econd and gather channel info.
      for (size_t i = 0; i < econdInfo.metadata().size(); ++i) {
        const auto &metadata = econdInfo.metadata()[i];

        // Use detId, adc, tot to compute calibration parameters for the corresponding channel.
        // Store the calibration parameters in a data structure that can be passed to the model.
      }

      for(auto it : moduleIndexer.typecodeMap()) {
        uint32_t fedid = it.second.first;
        uint32_t imod = it.second.second;
        uint32_t dqmIndex = moduleIndexer.getIndexForModule(fedid, imod);
        std::string typecode = it.first;
        std::replace(typecode.begin(), typecode.end(), '-', '_');
        typecodes[dqmIndex] = typecode;
        // module information
        auto modInfo = moduleInfo.view()[dqmIndex];
        // (u,v) coordinates = (i1, i2)
        // borrowed for HGCAL Map
        MonitoredElement_t ele;
        ele.typecode = typecode;
        ele.nErx = moduleIndexer.getNumERxs(fedid, imod);
        ele.zside = modInfo.zside();
        ele.endcap = ele.zside ? 1 : -1;
        ele.isSiPM = modInfo.isSiPM();
        ele.irot = modInfo.irot();
        ele.plane = modInfo.plane();
        ele.layer = modInfo.plane() * ele.endcap; // directional layer
        ele.i1 = modInfo.i1();
        ele.i2 = modInfo.i2();
        ele.x0y0 = getModuleCenter(file, ele.isSiPM, ele.plane, ele.i1, ele.i2);
        ele.fedid = fedid;
        ele.modid = imod;
        ele.econdidx = modInfo.econdidx();
        ele.cassette = getModuleCassette(file, modInfo.isSiPM(), modInfo.plane(), modInfo.i1(), modInfo.i2());
        ele.dqmIndex = dqmIndex;

        MonitoredElementKey_t key(fedid, imod)
        followedModules_[key] = ele; // Needed fo digis implify later.

        // My defined quantities
        int nTOA = 0; // Currently these requrie unpacking the digis.
        int nTOT = 0; // Currently these requrie unpacking the digis.
        const auto econd = econdInfo->const_view()[imod];


        float sum_cm = 0.0f;
        for (uint32_t erxIdx=0; erxIdx<mod.nErx; ++erxIdx) {
          uint16_t cm0 = econd.cm()(erxIdx, 0);
          uint16_t cm1 = econd.cm()(erxIdx, 1);
          uint32_t idx = erxIdx*2;
          sum_cm += cm0 + cm1;
        }
        for (uint32_t erxIdx=0; erxIdx<mod.nErx; ++erxIdx) {
          for (int cmIdx=0; cmIdx<2; ++cmIdx) {
            float cm_value = econd.cm()(erxIdx, cmIdx);
            float mean_sub_channel_index = cmIdx - 0.5 // mean index is 0.5 for 2 channels. This is wrong looks like it goes for 0-221
            float mean_sub_erx_index = erxIdx - 5.5 // mean index is 5.5 fo 12 erx
            float cell_area_fraction = 0.5 // Look up ho wot do this later.
          }
        }

      

    nch_per_erx = 37; // This should be a constant.
    // I think the indo is in the digis.
    for (int32_t i = 0; i < ndigis; ++i) {
      nCellsEntries++;
      //skip digis not tracked
      auto indexinfo = denseIndexInfo_view[i];
      
      MonitoredElementKey_t key(indexinfo.fedId(), indexinfo.fedReadoutSeq());
      if(followedModules_.find(key) == followedModules_.end()) {
        nNotFollowed++;
        continue;
      }
      // I think we use this key to find it's module, I CAN
      auto mod = followedModules_[key];
      // I need to know how to find it's erx?

      // Unconnected channels should have [8, 17, 19, 28] index wr.t
      //channel index
      uint32_t chIdx = indexinfo.chNumber();
      double tot = digi.tot();
      double toa = digi.toa();
      nTOA += (toa > 0) ? 1 : 0;
      nTOT += (tot > 0) ? 1 : 0;
      double cmsum = digi.cm();
      float cell_area_fraction = 0.5 // Look up ho wot do this later.
      // Get the ERX indx
      float mean_sub_channel_index = cmIdx - 221/2
      int erxIdc = chIdx / nch_per_erx;
      int chStartIdxWithinERX = chIdx % nch_per_erx;
      if (chStartIdxWithinERX == 8 || chStartIdxWithinERX == 17 || chStartIdxWithinERX == 19 || chStartIdxWithinERX == 28) {
        // This is an unconnected channel, handle accordingly
      } else {
        // This is a connected channel, handle accordingly
      }
      // For the four unconnected I need to use the cell area  == ) I think.
    }

    std::array<int, 2> getNtoaNtot(device::Event &event, const device::EventSetup &event_setup) override {
      // NTOA and NDIGIS used for all modules do it here first
      // We could parrelise this if needed.
      int nTOA = 0;
      int nTOT = 0;
      const auto digis_view = event.getHandle(digisTkn_)->const_view();
      int32_t ndigis = digis_view.metadata().size();
      for (int32_t i = 0; i < ndigis; ++i){
        auto digi = digis_view[i];
        double tot = digi.tot();
        double toa = digi.toa();
        nTOA += (toa > 0) ? 1 : 0;
        nTOT += (tot > 0) ? 1 : 0;
      }
      return {nTOA, nTOT};
    }

    void applyCorrections(device::Event &event, const device::EventSetup &event_setup) override {
      int nTOA, nTOT;
      std::tie(nTOA, nTOT) = getNtoaNtot(event, event_setup);
    }


    void produce(device::Event &event, const device::EventSetup &event_setup) override {
      // Channel info = 
      // in/out collections
      const auto batch_size = particles.const_view().metadata().size();
      auto regression_collection = portabletest::SimpleNetDeviceCollection(event.queue(), batch_size);

      // records
      auto input_records = particles.const_view().records();
      auto output_records = regression_collection.view().records();
      // input tensor definition
      cms::torch::alpakatools::TensorCollection<Queue> inputs(batch_size);
      inputs.add<portabletest::ParticleSoA>("particles", input_records.pt(), input_records.eta(), input_records.phi());
      // output tensor definition
      cms::torch::alpakatools::TensorCollection<Queue> outputs(batch_size);
      outputs.add<portabletest::SimpleNetSoA>("regression_head", output_records.reco_pt());

      model_.forward(event.queue(), inputs, outputs);
      // put device-side product into event
      event.emplace(simple_net_token_, std::move(regression_collection));
    }
    
  private:
    // event query tokens
    const device::EDGetToken<portabletest::ParticleDeviceCollection> particles_token_;
    const device::EDPutToken<portabletest::SimpleNetDeviceCollection> simple_net_token_;
    // model
    torch::AlpakaModel model_;
    // debug mode flag
    const ::torchtest::Environment environment_;
  };

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE::torchtest

DEFINE_FWK_ALPAKA_MODULE(torchtest::SimpleNet);