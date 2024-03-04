// -*- C++ -*-
//
// Package:    HGCalEmptyEventFilter
// Class:      HGCalEmptyEventFilter
//
/**\class HGCalEmptyEventFilter HGCalEmptyEventFilter.cc filter/HGCalEmptyEventFilter/src/HGCalEmptyEventFilter.cc

Description: <one line class summary>

Implementation:
<Notes on implementation>
*/
//
// Original Author: Pedro Thu Jul 13 CET 2023, based on Jeremiah Mans Tue Jun 4 CET 2012
//
//

// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"

#include "FWCore/Framework/interface/global/EDFilter.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/EmptyGroupDescription.h"

#include <string>
#include <iostream>

#include "DataFormats/FEDRawData/interface/FEDRawData.h"
#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"


//
// class declaration
//

class HGCalEmptyEventFilter : public edm::global::EDFilter<> {
public:
  explicit HGCalEmptyEventFilter(const edm::ParameterSet&);

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("hgcalEmulatedSlinkRawData","hgcalFEDRawData"));
    desc.add<std::vector<unsigned int> >("fedIds", {});
    descriptions.addWithDefaultLabel(desc);
  }


private:
  bool filter(edm::StreamID, edm::Event&, const edm::EventSetup&) const override;

  // ----------member data ---------------------------

  edm::EDGetTokenT<FEDRawDataCollection> tok_data_;
  const std::vector<unsigned int> fedIds_;
};

//
// constructors and destructor
//
HGCalEmptyEventFilter::HGCalEmptyEventFilter(const edm::ParameterSet& iConfig) :
  tok_data_(consumes<FEDRawDataCollection>(iConfig.getParameter<edm::InputTag>("src"))),
  fedIds_(iConfig.getParameter<std::vector<unsigned int> >("fedIds"))
{
  //now do what ever initialization is needed
}

//
// member functions
//

// ------------ method called on each new Event  ------------
bool HGCalEmptyEventFilter::filter(edm::StreamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const {
  using namespace edm;

  //skip if handle is invalid
  edm::Handle<FEDRawDataCollection> rawdata;
  iEvent.getByToken(tok_data_, rawdata);
  if(!rawdata.isValid()) return false;

  //if a fed is not empty that's good enough
  for (const auto& fed_id : fedIds_) {
    const auto& fed_data = rawdata->FEDData(fed_id);
    if(fed_data.size()>0) return true;
  }

  //nothing worh was found
  return false;
}

//define this as a plug-in
DEFINE_FWK_MODULE(HGCalEmptyEventFilter);
