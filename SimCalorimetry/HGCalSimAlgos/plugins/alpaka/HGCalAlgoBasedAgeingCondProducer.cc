#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/ESGetToken.h"

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ModuleFactory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "CondFormats/DataRecord/interface/HGCalDenseIndexInfoRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalMappingParameterDevice.h"
#include "CondFormats/HGCalObjects/interface/HGCalAgeingCondsHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalAgeingCondsDevice.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcal {

    class HGCalAlgoBasedAgeingCondProducer : public ESProducer {
    public:
      //
      HGCalAlgoBasedAgeingCondProducer(const edm::ParameterSet& iConfig) : ESProducer(iConfig) {
        auto cc = setWhatProduced(this);
        denseIndexTkn_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("denseIndexer"));
      }

      //
      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<edm::ESInputTag>("denseIndexer", edm::ESInputTag(""))->setComment("Dense indexer SoA source");
        descriptions.addWithDefaultLabel(desc);
      }

      //
      std::optional<HGCalAgeingCondsHost> produce(const HGCalDenseIndexInfoRcd& iRecord) {

        //get the dense indexer in use
        auto const& denseIndexInfo = iRecord.get(denseIndexTkn_);
        auto const& denseIndexInfo_view = denseIndexInfo.const_view();

        //declare the dense index info collection to be produced
        //the size is determined by the module indexer
        const int nIndices = denseIndexInfo_view.metadata().size();
        HGCalAgeingCondsHost ageingConds(cms::alpakatools::host(), nIndices);
        auto ac_view = ageingConds.view();
        for(int i=0; i<nIndices; i++){
          
          //read the coordinates of this cell to compute dose and fluence
          auto di_row = denseIndexInfo_view[i];
          auto x = di_row.x();
          auto y = di_row.y();
          auto r = std::sqrt(x*x + y*y);
          auto z = di_row.z();


          ac_view[i].dose() = r;
          ac_view[i].fluence() = z;          
        }

        //return denseIdxInfo;
        return ageingConds;
      }  // end of produce()

    private:
      edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexTkn_;
    };

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalAlgoBasedAgeingCondProducer);
