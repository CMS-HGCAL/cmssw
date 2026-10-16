#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationPayload.h"

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
#include <numeric>
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

  std::vector<int> readChannelVector(const json& moduleData,
                                     std::size_t nrows,
                                     const std::string& typecode) {
    if (!moduleData.contains("Channel")) {
      std::vector<int> channels(nrows);
      std::iota(channels.begin(), channels.end(), 0);
      return channels;
    }

    if (!moduleData.at("Channel").is_array()) {
      throw cms::Exception("InvalidData")
          << "Field 'Channel' for module '" << typecode
          << "' is not an array.";
    }

    auto channels = moduleData.at("Channel").get<std::vector<int>>();

    if (channels.size() < nrows) {
      throw cms::Exception("InvalidData")
          << "Field 'Channel' for module '" << typecode
          << "' contains only " << channels.size()
          << " values, but at least " << nrows << " are required.";
    }

    if (channels.size() > nrows) {
      edm::LogWarning("HGCalCalibrationDBWriter")
          << "Field 'Channel' for module '" << typecode
          << "' contains " << channels.size() << " values; using the first "
          << nrows << ".";
      channels.resize(nrows);
    }

    return channels;
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

  std::vector<float> readFlattenedMatrix(const json& moduleData,
                                         const std::string& key,
                                         std::size_t nrows,
                                         std::size_t width,
                                         const std::string& typecode,
                                         bool required = false) {
    if (!moduleData.contains(key)) {
      if (required) {
        throw cms::Exception("InvalidData")
            << "Missing required key '" << key
            << "' for module '" << typecode << "'.";
      }

      return std::vector<float>(nrows * width, 0.f);
    }

    const auto& matrix = moduleData.at(key);

    if (!matrix.is_array() || matrix.size() < nrows) {
      throw cms::Exception("InvalidData")
          << "Field '" << key << "' for module '" << typecode
          << "' must contain at least " << nrows << " rows.";
    }

    if (matrix.size() > nrows) {
      edm::LogWarning("HGCalCalibrationDBWriter")
          << "Field '" << key << "' for module '" << typecode
          << "' contains " << matrix.size() << " rows; using the first "
          << nrows << ".";
    }

    std::vector<float> flattened;
    flattened.reserve(nrows * width);

    for (std::size_t rowIndex = 0; rowIndex < nrows; ++rowIndex) {
      const auto& row = matrix.at(rowIndex);

      if (!row.is_array() || row.size() != width) {
        throw cms::Exception("InvalidData")
            << "Field '" << key << "' for module '" << typecode
            << "', row " << rowIndex << " contains "
            << row.size() << " values; expected " << width << ".";
      }

      const auto rowValues = row.get<std::vector<float>>();
      flattened.insert(flattened.end(),
                       rowValues.begin(),
                       rowValues.end());
    }

    return flattened;
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

  std::string recordName_;
  HGCalCalibrationPayload payload_;
};

HGCalCalibrationDBWriter::HGCalCalibrationDBWriter(
    const edm::ParameterSet& config)
    : recordName_(config.getParameter<std::string>("recordName")) {
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

  payload_.modules.reserve(calibrationData.size());

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

    HGCalCalibrationModulePayload module;
    module.typecode = typecode;

    module.channel = readChannelVector(moduleData, nrows, typecode);

    module.adcPed =
        readFloatVector(moduleData, "ADC_ped", nrows, typecode);
    module.noise =
        readFloatVector(moduleData, "Noise", nrows, typecode);
    module.cmPed =
        readFloatVector(moduleData, "CM_ped", nrows, typecode);
    module.cmSlope =
        readFloatVector(moduleData, "CM_slope", nrows, typecode);

    module.bxm1Slope =
        readFloatVector(moduleData, "BXm1_slope", nrows, typecode);
    module.bxm1Ped =
        readFloatVector(moduleData,
                        "BXm1_ped",
                        nrows,
                        typecode,
                        false);

    module.totToADC =
        readFloatVector(moduleData, "TOTtoADC", nrows, typecode);
    module.totPed =
        readFloatVector(moduleData, "TOT_ped", nrows, typecode);
    module.totLin =
        readFloatVector(moduleData, "TOT_lin", nrows, typecode);
    module.totP0 =
        readFloatVector(moduleData, "TOT_P0", nrows, typecode);
    module.totP1 =
        readFloatVector(moduleData, "TOT_P1", nrows, typecode);
    module.totP2 =
        readFloatVector(moduleData, "TOT_P2", nrows, typecode);

    module.toaCTDC = readFlattenedMatrix(
        moduleData,
        "TOA_CTDC",
        nrows,
        HGCalCalibrationModulePayload::kCTDCSize,
        typecode);

    module.toaFTDC = readFlattenedMatrix(
        moduleData,
        "TOA_FTDC",
        nrows,
        HGCalCalibrationModulePayload::kFTDCSize,
        typecode);

    module.toaTW = readFlattenedMatrix(
        moduleData,
        "TOA_TW",
        nrows,
        HGCalCalibrationModulePayload::kTWSize,
        typecode);

    module.mipsScale =
        readFloatVector(moduleData, "MIPS_scale", nrows, typecode);

    module.valid =
        readValidVector(moduleData, nrows, typecode);

    payload_.modules.push_back(std::move(module));
  }

  edm::LogInfo("HGCalCalibrationDBWriter")
      << "Parsed typed calibration payload for "
      << payload_.modules.size() << " module typecodes.";
}

void HGCalCalibrationDBWriter::endJob() {
  edm::Service<cond::service::PoolDBOutputService> poolDBService;

  if (!poolDBService.isAvailable()) {
    throw cms::Exception("Configuration")
        << "PoolDBOutputService is not configured.";
  }

  poolDBService->createOneIOV<HGCalCalibrationPayload>(
      payload_,
      poolDBService->beginOfTime(),
      recordName_);

  edm::LogInfo("HGCalCalibrationDBWriter")
      << "Stored HGCalCalibrationPayload in record '"
      << recordName_ << "'.";
}

void HGCalCalibrationDBWriter::fillDescriptions(
    edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription description;

  description
      .add<std::string>("jsonFile", "-")
      ->setComment(
          "Calibration JSON input file; use '-' to read from stdin.");

  description
      .add<std::string>("recordName", "HGCalCalibrationRcd")
      ->setComment("Conditions record used by PoolDBOutputService.");

  descriptions.addWithDefaultLabel(description);
}

DEFINE_FWK_MODULE(HGCalCalibrationDBWriter);
