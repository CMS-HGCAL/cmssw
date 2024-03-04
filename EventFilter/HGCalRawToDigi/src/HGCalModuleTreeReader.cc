/****************************************************************************
 *
 * This is a part of HGCAL offline software.
 * Authors:
 *   Pedro Silva, CERN
 *   Laurent Forthomme, CERN
 *
 ****************************************************************************/

#include "DataFormats/HGCalDigi/interface/HGCROCChannelDataFrame.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalModuleTreeReader.h"
#include "EventFilter/HGCalRawToDigi/interface/HGCalRawDataDefinitions.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "TChain.h"

using namespace hgcal::econd;

HGCalModuleTreeReader::HGCalModuleTreeReader(const EmulatorParameters& params,
                                             const std::string& tree_name,
                                             const std::vector<std::string>& filenames)
    : Emulator(params) {
  TChain chain(tree_name.data());
  for (const auto& filename : filenames)
    chain.Add(filename.c_str());

  HGCModuleTreeEvent event;
  chain.SetBranchAddress("event", &event.event);
  chain.SetBranchAddress("chip", &event.chip);
  chain.SetBranchAddress("half", &event.half);
  chain.SetBranchAddress("daqdata", &event.daqdata);
  chain.SetBranchAddress("bxcounter", &event.bxcounter);
  chain.SetBranchAddress("eventcounter", &event.eventcounter);
  chain.SetBranchAddress("orbitcounter", &event.orbitcounter);
  chain.SetBranchAddress("trigtime", &event.trigtime);
  chain.SetBranchAddress("trigwidth", &event.trigwidth);

  //events will be sequential for each eRx but in the tree the eRx will come at different entries
  //an alignment is done by creating a map where the key is the eRx id
  //and the contents are the sequence of events as pair<EventId, 32b data vector>
  for (long long i = 0; i < chain.GetEntries(); ++i) {

    chain.GetEntry(i);

    //start a new vector of events if not yet found
    ERxId_t erxKey{(uint8_t)event.chip, (uint8_t)event.half};
    if(data_.count(erxKey)==0) {
      std::vector< std::tuple<EventId, ERxData> > erxinputColl;
      data_[erxKey]=erxinputColl;

      std::vector< std::tuple<EventId, HGCalTestSystemMetaData> > erxmetadataColl;
      metadata_[erxKey]=erxmetadataColl;
    }

    //event identifier (counters are in practice <32b - see ROC documentation)
    EventId key{(uint32_t)event.eventcounter, (uint32_t)event.bxcounter, (uint32_t)event.orbitcounter};
    
    //add meta data
    HGCalTestSystemMetaData md(0,0,event.trigtime,event.trigwidth);
    metadata_[erxKey].push_back( std::tuple<EventId,HGCalTestSystemMetaData>(key,md) );
    
    //fill the data
    hgcal::econd::ERxData newInput;
 
    // daqdata: header, CM, 37 ch, CRC32, idle
    if (const auto nwords = event.daqdata->size(); nwords != 41)
      throw cms::Exception("HGCalModuleTreeReader")
          << "Invalid number of words retrieved for event {" << event.eventcounter << ":" << event.bxcounter << ":"
          << event.orbitcounter << "}: should be 41, got " << nwords << ".";

    // 1st word is the header: discard
    // 2nd word are the common mode words
    const uint32_t cmword(event.daqdata->at(1));
    if (((cmword >> 20) & 0xfff) != 0)
      throw cms::Exception("HGCalModuleTreeReader")
        << "Consistency check failed for common mode word: " << ((cmword >> 20) & 0xfff) << " != 0.";
    newInput.cm1 = cmword & 0x3ff;
    newInput.cm0 = (cmword >> 10) & 0x3ff;

    // next 37 words are channel data
    for (size_t i = 2; i < 2 + params_.num_channels_per_erx; i++) {
      newInput.rawROCword.push_back(event.daqdata->at(i));
    }

    // copy CRC32
    newInput.crc32 = event.daqdata->at(params_.num_channels_per_erx+2);

    //add data
    data_[erxKey].push_back( std::tuple<EventId,ERxData>(key,newInput) );

  }

  //check now that we have
  // -a consistent number of erx
  // -all have the same size
  // -the event ids are the same
  size_t nerxenabled=params_.enabled_erxs.size();
  assert(nerxenabled==data_.size());

  std::set<uint32_t> erxEventSizes;
  for(auto it: data_)
    erxEventSizes.insert( it.second.size() );
  assert(erxEventSizes.size()==1);

  totalEvents_ = *(erxEventSizes.begin());
  uint32_t noutofsync(0);
  auto beginit = data_.begin();
  for(auto it = data_.begin(); it != data_.end(); it++) {
    if(it==beginit) continue;
    for(uint32_t i=0; i<totalEvents_; i++) {
      auto erx0_event=std::get<0>(beginit->second[i]);
      auto erxi_event=std::get<0>(it->second[i]);
      if( std::get<0>(erx0_event) != std::get<0>(erxi_event)
          || std::get<1>(erx0_event) != std::get<1>(erxi_event) 
          || std::get<2>(erx0_event) != std::get<2>(erxi_event) )
        noutofsync++;
    }
  }
      
  edm::LogWarning("HGCalModuleTreeReader") << "read " << data_.size() << " eRx corresponding to " << totalEvents_ << " events";
  if(noutofsync>0)
    edm::LogWarning("HGCalModuleTreeReader") << "warning: detected " << noutofsync << " eRx out-of-sync event counters: check in your analysis";
  iEvent_ = 0;
}

//
std::unique_ptr<ECONDInput> HGCalModuleTreeReader::next() {

  if (iEvent_ == totalEvents_)
    throw cms::Exception("HGCalModuleTreeReader") << "Insufficient number of events were retrieved from input tree to proceed with the generation of emulated events.";

  //create the ECONDInput
  ERxInput erxinput;
  for(auto it = data_.begin(); it != data_.end(); it++)
    erxinput[it->first] = std::get<1>(it->second[iEvent_]);
  EventId eid = std::get<0>((data_.begin())->second[0]);
  auto data = std::make_unique<ECONDInput>(eid,erxinput);

  ++iEvent_;
  return data;
}

//
HGCalTestSystemMetaData HGCalModuleTreeReader::nextMetaData() {

  if(iEvent_==0 || iEvent_>totalEvents_)
    throw cms::Exception("HGCalModuleTreeReader") << "Insufficient number of events were retrieved from input tree to proceed with the generation of metadata for event emulation";

  //iEvent will have been incremented by 1 so metadata is retrieved from iEvent_-1
  auto imdEvent = iEvent_-1;
  return std::get<1>(metadata_.begin()->second[imdEvent]);
}
