// HGCal calibration conditions reader:
// HGCalCalibrationPayload from CondDB -> HGCalCalibParamHost

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
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationPayload.h"
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

      const HGCalCalibrationModulePayload& findModulePayload(
          const std::string& moduleTypecode,
          const HGCalCalibrationPayload& payload) {
        if (payload.modules.empty()) {
          throw cms::Exception("InvalidData")
              << "HGCalCalibrationPayload contains no module entries.";
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
            << "Could not find a calibration payload matching module '"
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

      void checkFlattenedMatrixSize(const std::vector<float>& values,
                                    std::size_t rows,
                                    std::size_t width,
                                    const std::string& field,
                                    const std::string& typecode) {
        const std::size_t expected = rows * width;

        if (values.size() != expected) {
          throw cms::Exception("InvalidData")
              << "Conditions field '" << field << "' for module '"
              << typecode << "' contains " << values.size()
              << " flattened values; expected " << expected
              << " (" << rows << " x " << width << ").";
        }
      }

      template <typename EigenRow>
      void fillFlattenedRow(EigenRow& output,
                            const std::vector<float>& values,
                            std::size_t row,
                            std::size_t width) {
        const std::size_t offset = row * width;

        for (std::size_t column = 0; column < width; ++column) {
          output(column) = values[offset + column];
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

        payloadToken_ =
            collector.consumes<HGCalCalibrationPayload>(
                configuration.getParameter<edm::ESInputTag>(
                    "payloadSource"));

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
                "HGCalCalibrationPayload from HGCalCalibrationRcd.");

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
        const auto& calibrationPayload =
            record.get(payloadToken_);

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

          const auto& module =
              findModulePayload(
                  moduleTypecode,
                  calibrationPayload);

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
                  module.adcPed.size());

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

          checkVectorSize(
              module.noise,
              module.adcPed.size(),
              "Noise",
              module.typecode);

          checkVectorSize(
              module.cmSlope,
              module.adcPed.size(),
              "CM_slope",
              module.typecode);

          checkVectorSize(
              module.cmPed,
              module.adcPed.size(),
              "CM_ped",
              module.typecode);

          checkVectorSize(
              module.bxm1Slope,
              module.adcPed.size(),
              "BXm1_slope",
              module.typecode);

          checkVectorSize(
              module.totToADC,
              module.adcPed.size(),
              "TOTtoADC",
              module.typecode);

          checkVectorSize(
              module.totPed,
              module.adcPed.size(),
              "TOT_ped",
              module.typecode);

          checkVectorSize(
              module.totLin,
              module.adcPed.size(),
              "TOT_lin",
              module.typecode);

          checkVectorSize(
              module.totP0,
              module.adcPed.size(),
              "TOT_P0",
              module.typecode);

          checkVectorSize(
              module.totP1,
              module.adcPed.size(),
              "TOT_P1",
              module.typecode);

          checkVectorSize(
              module.totP2,
              module.adcPed.size(),
              "TOT_P2",
              module.typecode);

          checkVectorSize(
              module.mipsScale,
              module.adcPed.size(),
              "MIPS_scale",
              module.typecode);

          checkVectorSize(
              module.valid,
              module.adcPed.size(),
              "Valid",
              module.typecode);

          checkFlattenedMatrixSize(
              module.toaCTDC,
              module.adcPed.size(),
              HGCalCalibrationModulePayload::kCTDCSize,
              "TOA_CTDC",
              module.typecode);

          checkFlattenedMatrixSize(
              module.toaFTDC,
              module.adcPed.size(),
              HGCalCalibrationModulePayload::kFTDCSize,
              "TOA_FTDC",
              module.typecode);

          checkFlattenedMatrixSize(
              module.toaTW,
              module.adcPed.size(),
              HGCalCalibrationModulePayload::kTWSize,
              "TOA_TW",
              module.typecode);

          fill_SoA_column<float>(
              product.view().ADC_ped(),
              module.adcPed,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().Noise(),
              module.noise,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().CM_slope(),
              module.cmSlope,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().CM_ped(),
              module.cmPed,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().BXm1_slope(),
              module.bxm1Slope,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().TOTtoADC(),
              module.totToADC,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().TOT_ped(),
              module.totPed,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().TOT_lin(),
              module.totLin,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().TOT_P0(),
              module.totP0,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().TOT_P1(),
              module.totP1,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().TOT_P2(),
              module.totP2,
              offset,
              rows);

          fill_SoA_column<float>(
              product.view().MIPS_scale(),
              module.mipsScale,
              offset,
              rows);

          fill_SoA_column<unsigned char>(
              product.view().valid(),
              module.valid,
              offset,
              rows);

          for (std::size_t channel = 0;
               channel < rows;
               ++channel) {
            auto output =
                product.view()[offset + channel];

            fillFlattenedRow(
                output.TOA_CTDC(),
                module.toaCTDC,
                channel,
                HGCalCalibrationModulePayload::kCTDCSize);

            fillFlattenedRow(
                output.TOA_FTDC(),
                module.toaFTDC,
                channel,
                HGCalCalibrationModulePayload::kFTDCSize);

            fillFlattenedRow(
                output.TOA_TW(),
                module.toaTW,
                channel,
                HGCalCalibrationModulePayload::kTWSize);
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

      edm::ESGetToken<
          HGCalCalibrationPayload,
          HGCalCalibrationRcd>
          payloadToken_;

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
