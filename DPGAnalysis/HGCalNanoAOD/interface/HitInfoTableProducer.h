#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "DataFormats/Common/interface/View.h"
#include "CommonTools/Utils/interface/StringCutObjectSelector.h"
#include "DataFormats/GeometryVector/interface/GlobalPoint.h"

#include <vector>
#include <iostream>

template <typename T>
class HitInfoTableProducer : public edm::stream::EDProducer<> {
 public:
  HitInfoTableProducer(edm::ParameterSet const& params)
    : name_(params.getParameter<std::string>("name")),
    doc_(params.getParameter<std::string>("doc")),
    src_(consumes<T>(params.getParameter<edm::InputTag>("src"))),
    cut_(params.getParameter<std::string>("cut"), true) {
    produces<nanoaod::FlatTable>();
  }

  ~HitInfoTableProducer() override {}

  unsigned int virtual layer(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:layer") << "Virtual function layer is not implemented!";
  }

  unsigned int virtual siThickIndex(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:siThickIndex") << "Virtual function siThickIndex is not implemented!";
  }

  unsigned int virtual zside(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:zside") << "Virtual function zside is not implemented!";
  }

  float virtual eta(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:eta") << "Virtual function eta is not implemented!";
  }

  float virtual phi(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:phi") << "Virtual function phi is not implemented!";
  }

  std::pair<int, int> virtual uv(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:uv") << "Virtual function uv coordinates is not implemented!";
  }

  std::pair<int, int> virtual wafer(const typename T::value_type& hit) {
    throw cms::Exception("HitInfoTableProducer:wafer") << "Virtual function wafer coordinates is not implemented!";
  }

  void produce(edm::Event& iEvent, const edm::EventSetup& iSetup) override {

    edm::Handle<T> objs;
    iEvent.getByToken(src_, objs);

    std::vector<unsigned int> layervals;
    std::vector<int> sithickvals;
    std::vector<int> zsidevals;
    std::vector<float> etavals;
    std::vector<float> phivals;
    std::vector<int> uvals;
    std::vector<int> vvals;
    std::vector<int> waferuvals;
    std::vector<int> wafervvals;

    for (const auto& obj : *objs) {
      if (cut_(obj)) {
	sithickvals.emplace_back(siThickIndex(obj));
	layervals.emplace_back(layer(obj));
	zsidevals.emplace_back(zside(obj));
	etavals.emplace_back(eta(obj));
	phivals.emplace_back(phi(obj));
	uvals.emplace_back(uv(obj).first);
	vvals.emplace_back(uv(obj).second);
	waferuvals.emplace_back(wafer(obj).first);
	wafervvals.emplace_back(wafer(obj).second);
      }
    }

    auto tab = std::make_unique<nanoaod::FlatTable>(layervals.size(), name_, false, true);
    tab->addColumn<int>("u_wafer", uvals, "rechit u wafer coordinate");
    tab->addColumn<int>("v_wafer", vvals, "rechit v wafer coordinate");
    tab->addColumn<int>("u_cell", uvals, "rechit u cell coordinate");
    tab->addColumn<int>("v_cell", vvals, "rechit v cell coordinate");
    tab->addColumn<unsigned int>("layer", layervals, "rechit layer coordinate");
    tab->addColumn<float>("eta", etavals, "rechit eta coordinate");
    tab->addColumn<float>("phi", phivals, "rechit phi coordinate");
    tab->addColumn<int>("zside", zsidevals, "rechit z side coordinate");
    tab->addColumn<int>("siThickIndex", sithickvals, "rechit SiThickIndex");

    iEvent.put(std::move(tab));

  }

 protected:
  const std::string name_, doc_;
  const edm::EDGetTokenT<T> src_;
  const StringCutObjectSelector<typename T::value_type> cut_;

};
