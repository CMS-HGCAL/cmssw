#include "EventFilter/HGCalRawToDigi/interface/HGCalECONDEmulator.h"
#include "DataFormats/HGCalDigi/interface/HGCalDigiCollections.h"
#include "FWCore/FWLite/interface/AutoLibraryLoader.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSetReader/interface/ParameterSetReader.h"
#include "FWCore/Utilities/interface/FileInPath.h"
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <random>

// http://www.cplusplus.com/reference/random/linear_congruential_engine/
std::minstd_rand0 myrand;

//
HGCalECONDEmulator::HGCROCChannelCollection_t generateHalfChip(bool rocCharMode) {
  HGCalECONDEmulator::HGCROCChannelCollection_t chData;

  //two common mode words
  for (size_t icm = 0; icm < 2; icm++) {
    uint16_t cm = myrand() % 1024;
    HGCROCChannelDataFrameSpec aSample;
    aSample.fill(true, false, false, 0, cm, 0, 0);

    chData.push_back(aSample);
  }

  //36 channels + 1 calib channel
  for (size_t ich = 0; ich < 37; ich++) {
    uint16_t adc = myrand() % 1024;
    uint16_t adcm1 = myrand() % 1024;
    uint16_t tot = myrand() % 2048;
    uint16_t toa = myrand() % 1024;
    short tc = myrand() % 2;
    short tp = myrand() % 2;
    HGCROCChannelDataFrameSpec aSample;
    aSample.fill(rocCharMode, tc, tp, adcm1, adc, tot, toa);
    chData.push_back(aSample);
  }

  return chData;
}

//
// run for instance with:
//
// time HGCECONDEmulatorTest EventFilter/HGCalRawToDigi/test/HGCECONDEmulatorTest_cfi.py
//
// for a measureble amount of time taken

int main(int argc, char **argv) {
  //if passed at command line use new cfi
  std::string url("EventFilter/HGCalRawToDigi/test/HGCECONDEmulatorTests_cfi.py");
  if (argc > 1)
    url = argv[1];
  url = edm::FileInPath(url).fullPath();
  std::string algo("passthroughTest");
  if (argc > 2)
    algo = argv[2];

  const std::shared_ptr<edm::ParameterSet> &pset = edm::readPSetsFrom(url);
  const edm::ParameterSet &cfg = pset->getParameter<edm::ParameterSet>(algo);
  uint32_t repetitions = cfg.getParameter<uint32_t>("repetitions");
  bool rocCharMode = cfg.getParameter<bool>("rocCharMode");
  bool econdPassthrough = cfg.getParameter<bool>("econdPassthrough");
  uint32_t neRx = cfg.getParameter<uint32_t>("neRx");
  int32_t seed = cfg.getParameter<int32_t>("seed");
  if (seed < 0)
    seed = std::chrono::system_clock::now().time_since_epoch().count();

  std::cout << "Basic performance " << algo << " tests for HGCECONDEmulator" << std::endl
            << "\t + repetitions [int]: " << repetitions << std::endl
            << "\t + roc characterization mode: " << rocCharMode << std::endl
            << "\t + ECON-D pass through mode: " << econdPassthrough << std::endl;

  myrand.seed(seed);

  // do the trials: time/performance test and exploit randomisation to check
  for (uint32_t u = 0; u < repetitions; u++) {
    //generate eRx data
    HGCalECONDEmulator::ERxCollection_t erxData;
    for (uint32_t n = 0; n < neRx; n++) {
      erxData.push_back(generateHalfChip(rocCharMode));
    }
  }

  return 0;
}
