// HGCal calibration conditions reader:
// Five pedestal condition payloads from CondDB -> HGCalCalibParamHost

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/RegexMatch.h"

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ModuleFactory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"

#include "CondFormats/DataRecord/interface/HGCalCalibrationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"

#include "CondFormats/HGCalObjects/interface/HGCalCalibrationParameterHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalCalibrationParameterDevice.h"
#include "CondFormats/DataRecord/interface/HGCalADCPedestalsRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalADCPedestalsPayload.h"
#include "CondFormats/DataRecord/interface/HGCalNoiseRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalNoisePayload.h"
#include "CondFormats/DataRecord/interface/HGCalCommonModePedestalsRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCommonModePedestalsPayload.h"
#include "CondFormats/DataRecord/interface/HGCalCommonModeSlopeRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCommonModeSlopePayload.h"
#include "CondFormats/DataRecord/interface/HGCalCalibrationValidityRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationValidityPayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"

#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalESProducerTools.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcalrechit {

    using namespace ::hgcal;

    namespace {

      template <typename Payload>
      const auto& findModulePayload(const std::string& moduleTypecode,
                                    const Payload& payload,
                                    const char* condition) {
        if (payload.modules.empty()) {
          throw cms::Exception("InvalidData")
              << "Condition '" << condition << "' contains no module entries.";
        }

        // Preserve the JSON search_modkey behaviour:
        // first matching typecode or glob pattern wins.
        for (const auto& module : payload.modules) {
          const std::regex expression(edm::glob2reg(module.typecode));

          if (std::regex_match(moduleTypecode, expression)) {
            edm::LogInfo("HGCalCalibrationDBESProducer")
                << "Matched module '" << moduleTypecode
                << "' to conditions payload key '"
                << module.typecode << "'.";

            return module;
          }
        }

        throw cms::Exception("InvalidData")
            << "Could not find " << condition << " conditions matching module '"
            << moduleTypecode << "'.";
      }

      template <typename T>
      void checkVectorSize(const std::vector<T>& values,
                           std::size_t expected,
                           const std::string& field,
                           const std::string& typecode) {
        if (values.size() != expected) {
          throw cms::Exception("InvalidData")
              << "Conditions field '" << field << "' for module '"
              << typecode << "' contains " << values.size()
              << " entries; expected " << expected << ".";
        }
      }

    }  // namespace

    class HGCalCalibrationDBESProducer : public ESProducer {
    public:
      explicit HGCalCalibrationDBESProducer(
          const edm::ParameterSet& configuration)
          : ESProducer(configuration),
            filenameEnergy_(
                configuration.getParameter<edm::FileInPath>(
                    "filenameEnergyLoss")) {
        auto collector = setWhatProduced(this);

        const auto payloadSource = configuration.getParameter<edm::ESInputTag>("payloadSource");
        adcPedestalsToken_ = collector.consumesFrom<HGCalADCPedestalsPayload, HGCalADCPedestalsRcd>(payloadSource);
        noiseToken_ = collector.consumesFrom<HGCalNoisePayload, HGCalNoiseRcd>(payloadSource);
        commonModePedestalsToken_ = collector.consumesFrom<HGCalCommonModePedestalsPayload, HGCalCommonModePedestalsRcd>(payloadSource);
        commonModeSlopeToken_ = collector.consumesFrom<HGCalCommonModeSlopePayload, HGCalCommonModeSlopeRcd>(payloadSource);
        validityToken_ = collector.consumesFrom<HGCalCalibrationValidityPayload, HGCalCalibrationValidityRcd>(payloadSource);

        indexToken_ =
            collector.consumesFrom<HGCalMappingModuleIndexer,
                                   HGCalElectronicsMappingRcd>(
                configuration.getParameter<edm::ESInputTag>(
                    "indexSource"));

        mapToken_ =
            collector.consumesFrom<
                ::hgcal::HGCalMappingModuleParamHost,
                HGCalElectronicsMappingRcd>(
                configuration.getParameter<edm::ESInputTag>(
                    "mapSource"));
      }

      static void fillDescriptions(
          edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription description;

        description
            .add<edm::FileInPath>("filenameEnergyLoss")
            ->setComment(
                "JSON file containing energy-loss and thickness "
                "corrections.");

        description
            .add<edm::ESInputTag>(
                "payloadSource",
                edm::ESInputTag(""))
            ->setComment(
                "Input label shared by the five pedestal condition payloads.");

        description
            .add<edm::ESInputTag>(
                "indexSource",
                edm::ESInputTag(""))
            ->setComment("HGCal module indexer source.");

        description
            .add<edm::ESInputTag>(
                "mapSource",
                edm::ESInputTag(""))
            ->setComment("HGCal module mapping source.");

        descriptions.addWithDefaultLabel(description);
      }

      std::optional<::hgcalrechit::HGCalCalibParamHost> produce(
          const HGCalCalibrationRcd& record) {
        const auto& adcPedestalsPayload = record.get(adcPedestalsToken_);
        const auto& noisePayload = record.get(noiseToken_);
        const auto& commonModePedestalsPayload = record.get(commonModePedestalsToken_);
        const auto& commonModeSlopePayload = record.get(commonModeSlopeToken_);
        const auto& validityPayload = record.get(validityToken_);

        const auto& moduleIndexer =
            record.get(indexToken_);

        const auto& moduleMapper =
            record.get(mapToken_);

        const uint32_t totalChannels =
            moduleIndexer.maxDataSize();

        ::hgcalrechit::HGCalCalibParamHost product(
            cms::alpakatools::host(),
            totalChannels);

        std::ifstream energyFile(
            filenameEnergy_.fullPath().c_str());

        if (!energyFile.is_open()) {
          throw cms::Exception("FileOpenError")
              << "Could not open energy-loss file '"
              << filenameEnergy_.fullPath() << "'.";
        }

        json energyData =
            json::parse(
                energyFile,
                nullptr,
                true,
                /*ignore_comments=*/true);

        const std::vector<std::string> energyKeys = {
            "dEdx",
            "SF_thickness_Si",
            "SF_thickness_SiPM"};

        check_keys(
            energyData,
            energyKeys,
            filenameEnergy_.fullPath());

        const auto energyLosses =
            energyData["dEdx"].get<std::vector<float>>();

        if (energyLosses.size() != 47) {
          edm::LogError("HGCalCalibrationDBESProducer")
              << "Expected 47 energy-loss layers, but found "
              << energyLosses.size() << " in "
              << filenameEnergy_.fullPath() << ".";
        }

        for (const auto& [moduleTypecode, ids] :
             moduleIndexer.typecodeMap()) {
          const auto [fedId, moduleId] = ids;

          const auto& adcPedestals = findModulePayload(moduleTypecode, adcPedestalsPayload, "ADC_ped");
          const auto& noise = findModulePayload(moduleTypecode, noisePayload, "Noise");
          const auto& commonModePedestals = findModulePayload(moduleTypecode, commonModePedestalsPayload, "CM_ped");
          const auto& commonModeSlope = findModulePayload(moduleTypecode, commonModeSlopePayload, "CM_slope");
          const auto& validity = findModulePayload(moduleTypecode, validityPayload, "Valid");

          const uint32_t moduleIndex =
              moduleIndexer.getIndexForModule(
                  fedId,
                  moduleId);

          const uint32_t offset =
              moduleIndexer.getIndexForModuleData(
                  moduleTypecode);

          const uint32_t mappedChannels =
              moduleIndexer.getNumChannels(
                  moduleTypecode);

          uint32_t rows =
              static_cast<uint32_t>(
                  adcPedestals.adcPed.size());

          if (mappedChannels != rows) {
            edm::LogWarning(
                "HGCalCalibrationDBESProducer")
                << "Channel count differs for module '"
                << moduleTypecode << "': mapping has "
                << mappedChannels
                << ", conditions payload has "
                << rows << ".";

            rows = std::min(rows, mappedChannels);
          }

          checkVectorSize(noise.noise, adcPedestals.adcPed.size(), "Noise", moduleTypecode);
          checkVectorSize(commonModePedestals.cmPed, adcPedestals.adcPed.size(), "CM_ped", moduleTypecode);
          checkVectorSize(commonModeSlope.cmSlope, adcPedestals.adcPed.size(), "CM_slope", moduleTypecode);
          checkVectorSize(validity.valid, adcPedestals.adcPed.size(), "Valid", moduleTypecode);

          for (std::size_t channel = 0; channel < rows; ++channel) {
            auto output = product.view()[offset + channel];
            output.ADC_ped() = adcPedestals.adcPed[channel];
            output.Noise() = noise.noise[channel];
            output.CM_ped() = commonModePedestals.cmPed[channel];
            output.CM_slope() = commonModeSlope.cmSlope[channel];
            output.valid() = validity.valid[channel];

            // Match the defaults of PrepareLevel0CalibParams.py for pedestal input.
            output.BXm1_slope() = 0.f;
            output.TOTtoADC() = 1.f;
            output.TOT_ped() = 0.f;
            output.TOT_lin() = 0.f;
            output.TOT_P0() = 0.f;
            output.TOT_P1() = 0.f;
            output.TOT_P2() = 0.f;
            output.MIPS_scale() = 1.f;
            output.TOA_CTDC().setZero();
            output.TOA_FTDC().setZero();
            output.TOA_TW().setZero();
          }

          const int layer =
              moduleMapper.view().plane()[moduleIndex];

          if (layer <= 0 ||
              static_cast<std::size_t>(layer) >
                  energyLosses.size()) {
            throw cms::Exception("InvalidData")
                << "Invalid HGCal layer " << layer
                << " for module '" << moduleTypecode
                << "'.";
          }

          float energyLoss =
              energyLosses[layer - 1];

          const bool isSiPM =
              moduleMapper.view().isSiPM()[moduleIndex];

          const int cellType =
              moduleMapper.view().celltype()[moduleIndex];

          const uint32_t detectorId =
              moduleMapper.view().detid()[moduleIndex];

          float thicknessScale = 0.f;

          if (isSiPM) {
            thicknessScale =
                energyData["SF_thickness_SiPM"][0];
          } else {
            thicknessScale =
                getThicknessCorrection(
                    energyData["SF_thickness_Si"],
                    detectorId,
                    cellType);
          }

          if (thicknessScale <= 0.f) {
            throw cms::Exception("ConfigError")
                << "HGCal thickness scale factor is not positive.";
          }

          energyLoss *=
              (1.f / thicknessScale) * 1.e-3f;

          fill_SoA_column_single<float>(
              product.view().EM_scale().data(),
              energyLoss,
              offset,
              rows);
        }

        return product;
      }

    private:
      float getThicknessCorrection(
          const std::vector<float>& scaleFactors,
          uint32_t detectorId,
          int cellType) const {
        using waferType = HGCSiliconDetId::waferType;

        const HGCSiliconDetId detId(detectorId);
        const bool isCEE =
            detId.det() == DetId::HGCalEE;

        uint32_t index = 0;

        if (cellType == waferType::HGCalHD120) {
          index = isCEE ? 0 : 3;
        } else if (
            cellType == waferType::HGCalHD200 ||
            cellType == waferType::HGCalLD200) {
          index = isCEE ? 1 : 4;
        } else if (
            cellType == waferType::HGCalLD300) {
          index = isCEE ? 2 : 5;
        } else {
          throw cms::Exception("InvalidData")
              << "Could not determine thickness correction "
              << "for cell type " << cellType
              << ", layer " << detId.layer() << ".";
        }

        if (index >= scaleFactors.size()) {
          throw cms::Exception("InvalidData")
              << "Thickness-correction index " << index
              << " exceeds the vector size "
              << scaleFactors.size() << ".";
        }

        return scaleFactors[index];
      }

      edm::ESGetToken<HGCalADCPedestalsPayload, HGCalADCPedestalsRcd> adcPedestalsToken_;
      edm::ESGetToken<HGCalNoisePayload, HGCalNoiseRcd> noiseToken_;
      edm::ESGetToken<HGCalCommonModePedestalsPayload, HGCalCommonModePedestalsRcd> commonModePedestalsToken_;
      edm::ESGetToken<HGCalCommonModeSlopePayload, HGCalCommonModeSlopeRcd> commonModeSlopeToken_;
      edm::ESGetToken<HGCalCalibrationValidityPayload, HGCalCalibrationValidityRcd> validityToken_;

      edm::ESGetToken<
          HGCalMappingModuleIndexer,
          HGCalElectronicsMappingRcd>
          indexToken_;

      edm::ESGetToken<
          hgcal::HGCalMappingModuleParamHost,
          HGCalElectronicsMappingRcd>
          mapToken_;

      const edm::FileInPath filenameEnergy_;
    };

  }  // namespace hgcalrechit

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(
    hgcalrechit::HGCalCalibrationDBESProducer);
