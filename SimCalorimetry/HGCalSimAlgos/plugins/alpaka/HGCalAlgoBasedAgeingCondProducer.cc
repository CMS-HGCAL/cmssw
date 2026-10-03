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
#include "CondFormats/DataRecord/interface/HGCalAgeingCondsRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalAgeingCondsHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalAgeingCondsDevice.h"

#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalSiNoiseMap.h"

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
        
        //init the noise map for Si
        auto doseMapURL = iConfig.getParameter<edm::FileInPath>("doseMapURL");
        auto doseMapAlgo = iConfig.getParameter<uint32_t>("doseMapAlgo");
        auto scaleByDoseFactor = iConfig.getParameter<double>("scaleByDoseFactor");
        std::vector<double> ileakParam(iConfig.getParameter<edm::ParameterSet>("ileakParam").template getParameter<std::vector<double>>("ileakParam"));
        std::vector<double> cceParam120(iConfig.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParam120"));
        std::vector<double> cceParam200(iConfig.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParam200"));
        std::vector<double> cceParam300(iConfig.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParam300"));
        siNoiseMap_ = std::unique_ptr<HGCalSiNoiseMap<HGCSiliconDetId>>(new HGCalSiNoiseMap<HGCSiliconDetId>);
        siNoiseMap_->setDoseMap(doseMapURL.fullPath(), doseMapAlgo);
        siNoiseMap_->setFluenceScaleFactor(scaleByDoseFactor);
        siNoiseMap_->setIleakParam(ileakParam);
        siNoiseMap_->setCceParam(cceParam120, cceParam200, cceParam300);
      }

      //
      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<edm::ESInputTag>("denseIndexer", edm::ESInputTag(""))->setComment("Dense indexer SoA source");
        desc.add<edm::FileInPath>("doseMapURL")->setComment("dose map file");
        desc.add<uint32_t>("doseMapAlgo")->setComment("fluence algo to use");
        desc.add<double>("scaleByDoseFactor")->setComment("dose scaling factor");
        edm::ParameterSetDescription ileakDesc;
        ileakDesc.add<std::vector<double>>("ileakParam");
        desc.add<edm::ParameterSetDescription>("ileakParam", ileakDesc)->setComment("leakage current parameterization");
        edm::ParameterSetDescription cceDesc;
        cceDesc.add<std::vector<double>>("cceParam120");
        cceDesc.add<std::vector<double>>("cceParam200");
        cceDesc.add<std::vector<double>>("cceParam300");
        desc.add<edm::ParameterSetDescription>("cceParams", cceDesc)->setComment("charge collection efficiency parameterizations");
        descriptions.addWithDefaultLabel(desc);
      }

      //
      std::optional<HGCalAgeingCondsHost> produce(const HGCalAgeingCondsRcd& iRecord) {

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
          auto layer = di_row.layer();

          ac_view[i].dose() = r;
          ac_view[i].fluence() = layer;          
        }

        //return denseIdxInfo;
        return ageingConds;
      }  // end of produce()

    private:
      edm::ESGetToken<hgcal::HGCalDenseIndexInfoHost, HGCalDenseIndexInfoRcd> denseIndexTkn_;
      std::unique_ptr<HGCalSiNoiseMap<HGCSiliconDetId> > siNoiseMap_;
    };

  }  // namespace hgcal

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcal::HGCalAlgoBasedAgeingCondProducer);
