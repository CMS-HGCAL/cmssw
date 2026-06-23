#ifndef HGCalTools_PedestalDB_HGCalPedestalConditions_h
#define HGCalTools_PedestalDB_HGCalPedestalConditions_h

#include <vector>
#include <string>
#include <cstdint>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/nvp.hpp>

class HGCalPedestalConditions {
public:
  HGCalPedestalConditions() {}
  ~HGCalPedestalConditions() {}

  struct ModuleData {
    std::string typeCode;
    std::string reference;
    std::string timestamp;
    std::vector<int32_t> channel;
    std::vector<int32_t> valid;
    std::vector<float>   adc_ped;
    std::vector<float>   adc_rms;
    std::vector<float>   cm2_ped;
    std::vector<float>   cm2_rms;
    std::vector<float>   cm4_ped;
    std::vector<float>   cm4_rms;
    std::vector<float>   cm2_slope;
    std::vector<float>   cm4_slope;

    int nChannels() const { return (int)adc_ped.size(); }

  private:
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int) {
      ar & BOOST_SERIALIZATION_NVP(typeCode);
      ar & BOOST_SERIALIZATION_NVP(reference);
      ar & BOOST_SERIALIZATION_NVP(timestamp);
      ar & BOOST_SERIALIZATION_NVP(channel);
      ar & BOOST_SERIALIZATION_NVP(valid);
      ar & BOOST_SERIALIZATION_NVP(adc_ped);
      ar & BOOST_SERIALIZATION_NVP(adc_rms);
      ar & BOOST_SERIALIZATION_NVP(cm2_ped);
      ar & BOOST_SERIALIZATION_NVP(cm2_rms);
      ar & BOOST_SERIALIZATION_NVP(cm4_ped);
      ar & BOOST_SERIALIZATION_NVP(cm4_rms);
      ar & BOOST_SERIALIZATION_NVP(cm2_slope);
      ar & BOOST_SERIALIZATION_NVP(cm4_slope);
    }
  };

  std::vector<ModuleData> modules;
  int nModules() const { return (int)modules.size(); }

  const ModuleData* getModule(const std::string& typeCode) const {
    for (const auto& m : modules)
      if (m.typeCode == typeCode) return &m;
    return nullptr;
  }

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar & BOOST_SERIALIZATION_NVP(modules);
  }
};

#endif
