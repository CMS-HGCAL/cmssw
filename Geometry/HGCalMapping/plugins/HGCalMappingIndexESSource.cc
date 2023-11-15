#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESProducer.h"
#include "FWCore/Framework/interface/EventSetupRecordIntervalFinder.h"
#include "FWCore/Framework/interface/ESProducts.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "CondFormats/DataRecord/interface/HGCalMappingModuleIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiCellIndexerRcd.h"
#include "CondFormats/DataRecord/interface/HGCalMappingSiPMCellIndexerRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingCellIndexer.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

/**
   @short plugin parses the module/cell locator files to produce the indexer records
 */
class HGCalMappingIndexESSource : public edm::ESProducer, public edm::EventSetupRecordIntervalFinder {
public:
  explicit HGCalMappingIndexESSource(const edm::ParameterSet& iConfig)
    : module_filename_(iConfig.getParameter<std::string>("modules")),
      si_filename_(iConfig.getParameter<std::string>("si")),
      sipm_filename_(iConfig.getParameter<std::string>("sipm"))
  {

    setWhatProduced(this, &HGCalMappingIndexESSource::produceModules);
    setWhatProduced(this, &HGCalMappingIndexESSource::produceSi);
    setWhatProduced(this, &HGCalMappingIndexESSource::produceSiPM);

    findingRecord<HGCalMappingModuleIndexerRcd>();
    findingRecord<HGCalMappingSiCellIndexerRcd>();
    findingRecord<HGCalMappingSiPMCellIndexerRcd>();
  }

  std::unique_ptr<HGCalMappingModuleIndexer> produceModules(const HGCalMappingModuleIndexerRcd&);
  std::unique_ptr<HGCalMappingCellIndexer> produceSi(const HGCalMappingSiCellIndexerRcd&);
  std::unique_ptr<HGCalMappingCellIndexer> produceSiPM(const HGCalMappingSiPMCellIndexerRcd&);

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::string>("modules", "Geometry/HGCalMapping/data/modulelocator.txt");
    desc.add<std::string>("si", "Geometry/HGCalMapping/data/WaferCellMapTraces.txt");
    desc.add<std::string>("sipm", "Geometry/HGCalMapping/data/channels_sipmontile.hgcal.txt");
    descriptions.addWithDefaultLabel(desc);
  }

private:

  void setIntervalFor(const edm::eventsetup::EventSetupRecordKey&,
                      const edm::IOVSyncValue&,
                      edm::ValidityInterval& oValidity) override {
    oValidity = edm::ValidityInterval(edm::IOVSyncValue::beginOfTime(), edm::IOVSyncValue::endOfTime());
  }
  
  const std::string module_filename_, si_filename_, sipm_filename_;

};


//
std::unique_ptr<HGCalMappingModuleIndexer> HGCalMappingIndexESSource::produceModules(const HGCalMappingModuleIndexerRcd &rcd) {  

  // load module mapping parameters and find ranges
  edm::FileInPath fip(module_filename_);
  std::ifstream file(fip.fullPath());
  std::string line, SiPMtype;
  size_t iline(0);
  bool isSiPM,isHD;
  int plane, u, v, zside;
  uint16_t fedid,localfedid,wafType,captureblock,econdidx,captureblockidx;
  uint16_t maxlocalfedid(0),maxcaptureblock(0),maxecondidx(0),maxerx(0);
  while(std::getline(file, line))
    {
      iline++;
      if(iline==1) continue;
      
      std::istringstream stream(line);
      stream >> plane >> u >> v >> isSiPM >> isHD; 
      if(isSiPM) stream >> SiPMtype;
      else stream >> wafType;
      stream >> econdidx >> captureblock >> localfedid >> captureblockidx >> fedid >> zside;
      
      maxlocalfedid=std::max(localfedid,maxlocalfedid);
      maxcaptureblock=std::max(captureblock,maxcaptureblock);
      maxecondidx=std::max(econdidx,maxecondidx);
      uint16_t nerx=6*(1+isHD);
      maxerx=std::max(nerx,maxerx);
    }

  // configure module indexer and return
  auto c = std::make_unique<HGCalMappingModuleIndexer>();
  c->update(maxlocalfedid+1, maxcaptureblock+1, maxecondidx+1, maxerx+1);
  return c;
}


//
std::unique_ptr<HGCalMappingCellIndexer> HGCalMappingIndexESSource::produceSi(const HGCalMappingSiCellIndexerRcd &rcd) {

  // load Si cell specific module mapping parameters
  edm::FileInPath fip(si_filename_);
  std::ifstream file(fip.fullPath());
  std::string line;
  size_t iline(0);
  uint16_t maxtype(0),maxchip(0),maxhalf(0),maxseq(0);
  uint16_t type, chip, half;
  uint16_t seq;
  int sicell,triglink,trigcell,iu,iv,t;
  float trace;
  while(std::getline(file, line))
    {
      iline++;
      if(iline==1) continue;
      std::istringstream stream(line);
      
      std::string denscol,rocpincol;
      stream >> denscol;
      stream >> type >> chip >> half >> seq;
      stream >> rocpincol;
      stream >> sicell >> triglink >> trigcell >> iu >> iv >> trace >> t;
      
      maxtype=std::max(type,maxtype);
      maxchip=std::max(chip,maxchip);
      maxhalf=std::max(half,maxhalf);
      maxseq=std::max(seq,maxseq);
    }

  //instantiate the cell indexer with the appropriate ranges for the Si modules
  auto c = std::make_unique<HGCalMappingCellIndexer>();
  c->update(maxtype+1,maxchip+1,maxhalf+1,maxseq+1);
  return c;
}

//
std::unique_ptr<HGCalMappingCellIndexer> HGCalMappingIndexESSource::produceSiPM(const HGCalMappingSiPMCellIndexerRcd &rcd) {

  //instantiate the cell indexer 
  auto c = std::make_unique<HGCalMappingCellIndexer>();
  
  // load module mapping parameters
  edm::FileInPath fip(sipm_filename_);
  std::ifstream file(fip.fullPath());
  std::string line;
  size_t iline(0);
  uint16_t maxtype(0),maxchip(0),maxhalf(0),maxseq(0);
  int plane,iu,iv,trigcell,triglink,modiu,t;
  uint16_t type, index, chip, half, seq;
  std::string typestr;
  
  while(std::getline(file, line))
    {
      iline++;
      if(iline==1) continue;
      std::istringstream stream(line);
      
      stream >> index >> chip >> half >> seq >> plane >> iu >> iv >> typestr >> trigcell >> triglink >> modiu >> t;
      type = c->convertType(typestr);
      
      maxtype=std::max(type,maxtype);
      maxchip=std::max(chip,maxchip);
      maxhalf=std::max(half,maxhalf);
      maxseq=std::max(seq,maxseq);
      std::cout << "seq: " << seq << std::endl;
    }

  //update with the appropriate ranges for the tileboards
  c->update(maxtype+1,maxchip+1,maxhalf+1,maxseq+1);
  return c;
}

DEFINE_FWK_EVENTSETUP_SOURCE(HGCalMappingIndexESSource);
