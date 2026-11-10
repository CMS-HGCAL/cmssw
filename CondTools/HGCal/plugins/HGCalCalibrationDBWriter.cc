#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HGCalObjects/interface/HGCalADCPedestalsPayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalNoisePayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalCommonModePedestalsPayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalCommonModeSlopePayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationValidityPayload.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDAnalyzer.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

  using json = nlohmann::ordered_json;

  void checkModuleObject(const json& moduleData, const std::string& typecode) {
    if (!moduleData.is_object()) {
      throw cms::Exception("InvalidData")
          << "Calibration entry for module '" << typecode
          << "' is not a JSON object.";
    }
  }

  std::vector<float> readFloatVector(const json& moduleData,
                                     const std::string& key,
                                     std::size_t nrows,
                                     const std::string& typecode,
                                     bool required = true) {
    if (!moduleData.contains(key)) {
      if (required) {
        throw cms::Exception("InvalidData")
            << "Missing required key '" << key
            << "' for module '" << typecode << "'.";
      }

      return std::vector<float>(nrows, 0.f);
    }

    if (!moduleData.at(key).is_array()) {
      throw cms::Exception("InvalidData")
          << "Field '" << key << "' for module '" << typecode
          << "' is not an array.";
    }

    auto values = moduleData.at(key).get<std::vector<float>>();

    if (values.size() < nrows) {
      throw cms::Exception("InvalidData")
          << "Field '" << key << "' for module '" << typecode
          << "' contains only " << values.size()
          << " values, but at least " << nrows << " are required.";
    }

    if (values.size() > nrows) {
      edm::LogWarning("HGCalCalibrationDBWriter")
          << "Field '" << key << "' for module '" << typecode
          << "' contains " << values.size() << " values; using the first "
          << nrows << " to preserve the existing JSON reconstruction behaviour.";
      values.resize(nrows);
    }

    return values;
  }

  std::vector<unsigned char> readValidVector(const json& moduleData,
                                             std::size_t nrows,
                                             const std::string& typecode) {
    if (!moduleData.contains("Valid") ||
        !moduleData.at("Valid").is_array()) {
      throw cms::Exception("InvalidData")
          << "Missing or invalid 'Valid' array for module '"
          << typecode << "'.";
    }

    const auto input = moduleData.at("Valid").get<std::vector<int>>();

    if (input.size() < nrows) {
      throw cms::Exception("InvalidData")
          << "Field 'Valid' for module '" << typecode
          << "' contains only " << input.size()
          << " values, but at least " << nrows << " are required.";
    }

    if (input.size() > nrows) {
      edm::LogWarning("HGCalCalibrationDBWriter")
          << "Field 'Valid' for module '" << typecode
          << "' contains " << input.size() << " values; using the first "
          << nrows << ".";
    }

    std::vector<unsigned char> output;
    output.reserve(nrows);

    for (std::size_t channel = 0; channel < nrows; ++channel) {
      const int value = input[channel];

      if (value != 0 && value != 1) {
        throw cms::Exception("InvalidData")
            << "Field 'Valid' for module '" << typecode
            << "', channel index " << channel
            << " has value " << value << "; expected 0 or 1.";
      }

      output.push_back(static_cast<unsigned char>(value));
    }

    return output;
  }

}  // namespace

class HGCalCalibrationDBWriter : public edm::global::EDAnalyzer<> {
public:
  explicit HGCalCalibrationDBWriter(const edm::ParameterSet&);
  ~HGCalCalibrationDBWriter() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void analyze(edm::StreamID,
               const edm::Event&,
               const edm::EventSetup&) const override {}

  void endJob() override;

  HGCalADCPedestalsPayload adcPedestals_;
  HGCalNoisePayload noise_;
  HGCalCommonModePedestalsPayload commonModePedestals_;
  HGCalCommonModeSlopePayload commonModeSlope_;
  HGCalCalibrationValidityPayload validity_;
};

HGCalCalibrationDBWriter::HGCalCalibrationDBWriter(
    const edm::ParameterSet& config) {
  const std::string jsonFile =
      config.getParameter<std::string>("jsonFile");

  std::unique_ptr<std::ifstream> fileInput;
  std::istream* input = &std::cin;

  if (jsonFile != "-") {
    fileInput = std::make_unique<std::ifstream>(jsonFile);

    if (!fileInput->is_open()) {
      throw cms::Exception("FileOpenError")
          << "Could not open calibration JSON file '"
          << jsonFile << "'.";
    }

    input = fileInput.get();
  }

  json calibrationData;

  try {
    calibrationData =
        json::parse(*input, nullptr, true, /*ignore_comments=*/true);
  } catch (const json::exception& error) {
    throw cms::Exception("InvalidData")
        << "Failed to parse calibration JSON from '"
        << jsonFile << "': " << error.what();
  }

  if (!calibrationData.is_object() || calibrationData.empty()) {
    throw cms::Exception("InvalidData")
        << "The calibration input must be a non-empty JSON object.";
  }

  adcPedestals_.modules.reserve(calibrationData.size());
  noise_.modules.reserve(calibrationData.size());
  commonModePedestals_.modules.reserve(calibrationData.size());
  commonModeSlope_.modules.reserve(calibrationData.size());
  validity_.modules.reserve(calibrationData.size());

  for (const auto& item : calibrationData.items()) {
    const std::string typecode = item.key();
    const json& moduleData = item.value();

    checkModuleObject(moduleData, typecode);

    if (!moduleData.contains("ADC_ped") ||
        !moduleData.at("ADC_ped").is_array()) {
      throw cms::Exception("InvalidData")
          << "Missing or invalid 'ADC_ped' array for module '"
          << typecode << "'.";
    }

    // Match the existing JSON ESProducer: the first JSON field determines
    // the number of rows used for this module.
    const auto firstField = moduleData.begin();

    if (firstField == moduleData.end() || !firstField.value().is_array()) {
      throw cms::Exception("InvalidData")
          << "The first field for module '" << typecode
          << "' must be a channel-level array.";
    }

    const std::size_t nrows = firstField.value().size();

    if (nrows == 0) {
      throw cms::Exception("InvalidData")
          << "Module '" << typecode << "' contains no channels.";
    }

    // These conditions currently support the pedestal workflow only.
    // Reject calibrations that cannot be represented by these payloads.
    for (const auto* key : {"BXm1_slope", "BXm1_ped", "TOTtoADC", "TOT_ped",
                           "TOT_lin", "TOT_P0", "TOT_P1", "TOT_P2", "MIPS_scale"}) {
      if (!moduleData.contains(key))
        continue;
      const float expected = (std::string(key) == "TOTtoADC" || std::string(key) == "MIPS_scale") ? 1.f : 0.f;
      const auto values = readFloatVector(moduleData, key, nrows, typecode);
      if (std::any_of(values.begin(), values.end(), [expected](float value) { return value != expected; }))
        throw cms::Exception("InvalidData")
            << "Non-default " << key << " for module '" << typecode
            << "': this writer supports pedestal conditions only.";
    }

    // TOA corrections are outside the scope of these conditions.
    for (const auto* key : {"TOA_CTDC", "TOA_FTDC", "TOA_TW"}) {
      if (!moduleData.contains(key))
        continue;
      for (const auto& row : moduleData.at(key).get<std::vector<std::vector<float>>>()) {
        if (std::any_of(row.begin(), row.end(), [](float x) { return x != 0.f; }))
          throw cms::Exception("InvalidData")
              << "Nonzero " << key << " for module '" << typecode
              << "': TOA calibration is not supported by this payload.";
      }
    }

    {
      HGCalADCPedestalsModulePayload module;
      module.typecode = typecode;
      module.adcPed = readFloatVector(moduleData, "ADC_ped", nrows, typecode);
      adcPedestals_.modules.push_back(std::move(module));
    }
    {
      HGCalNoiseModulePayload module;
      module.typecode = typecode;
      module.noise = readFloatVector(moduleData, "Noise", nrows, typecode);
      noise_.modules.push_back(std::move(module));
    }
    {
      HGCalCommonModePedestalsModulePayload module;
      module.typecode = typecode;
      module.cmPed = readFloatVector(moduleData, "CM_ped", nrows, typecode);
      commonModePedestals_.modules.push_back(std::move(module));
    }
    {
      HGCalCommonModeSlopeModulePayload module;
      module.typecode = typecode;
      module.cmSlope = readFloatVector(moduleData, "CM_slope", nrows, typecode);
      commonModeSlope_.modules.push_back(std::move(module));
    }
    {
      HGCalCalibrationValidityModulePayload module;
      module.typecode = typecode;
      module.valid = readValidVector(moduleData, nrows, typecode);
      validity_.modules.push_back(std::move(module));
    }
  }

  edm::LogInfo("HGCalCalibrationDBWriter")
      << "Parsed five pedestal condition payloads for "
      << adcPedestals_.modules.size() << " module typecodes.";
}

void HGCalCalibrationDBWriter::endJob() {
  edm::Service<cond::service::PoolDBOutputService> poolDBService;

  if (!poolDBService.isAvailable()) {
    throw cms::Exception("Configuration")
        << "PoolDBOutputService is not configured.";
  }

  poolDBService->createOneIOV<HGCalADCPedestalsPayload>(
      adcPedestals_, poolDBService->beginOfTime(), "HGCalADCPedestalsRcd");
  poolDBService->createOneIOV<HGCalNoisePayload>(
      noise_, poolDBService->beginOfTime(), "HGCalNoiseRcd");
  poolDBService->createOneIOV<HGCalCommonModePedestalsPayload>(
      commonModePedestals_, poolDBService->beginOfTime(), "HGCalCommonModePedestalsRcd");
  poolDBService->createOneIOV<HGCalCommonModeSlopePayload>(
      commonModeSlope_, poolDBService->beginOfTime(), "HGCalCommonModeSlopeRcd");
  poolDBService->createOneIOV<HGCalCalibrationValidityPayload>(
      validity_, poolDBService->beginOfTime(), "HGCalCalibrationValidityRcd");

  edm::LogInfo("HGCalCalibrationDBWriter")
      << "Stored five pedestal condition payloads in separate records.";
}

void HGCalCalibrationDBWriter::fillDescriptions(
    edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription description;

  description
      .add<std::string>("jsonFile", "-")
      ->setComment(
          "Calibration JSON input file; use '-' to read from stdin.");

  descriptions.addWithDefaultLabel(description);
}

DEFINE_FWK_MODULE(HGCalCalibrationDBWriter);
