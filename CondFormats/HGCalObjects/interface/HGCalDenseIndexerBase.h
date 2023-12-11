#ifndef CondFormats_HGCalObjects_interface_HGCalDenseIndexerBase_h
#define CondFormats_HGCalObjects_interface_HGCalDenseIndexerBase_h

#include "FWCore/Utilities/interface/Exception.h"
#include <array>
#include <numeric>

/**
   @short this is a simple class that takes care of building a dense index for a set of categories
   the maximum number of items expected in each category is encoded in the IndexRanges_t
   the class is templated for the number of categories to use
 */
template<std::size_t N> 
class HGCalDenseIndexerBase {

 public:

  typedef std::array<uint32_t,N> IndexCodes_t;
  typedef std::array<uint32_t,N> IndexRanges_t;
  
  HGCalDenseIndexerBase() : maxIdx_(0) { vmax_.fill(0); }

  HGCalDenseIndexerBase(IndexRanges_t o) { updateRanges(o); }

  void updateRanges(IndexRanges_t o) {
    check(o.size());
    vmax_=o;
    maxIdx_ = std::accumulate(vmax_.begin(), vmax_.end(), 1, std::multiplies<uint32_t>());
  }
  
  uint32_t denseIndex(IndexCodes_t v) {
    uint32_t rtn = v[0];
    for(size_t i=1; i<N; i++)
      rtn = rtn * vmax_[i]+v[i];
    return rtn;
  }

  IndexCodes_t unpackDenseIndex(uint32_t rtn) {
    IndexCodes_t codes;

    const auto rend=vmax_.rend();
    for(auto rit = vmax_.rbegin(); rit != rend; ++rit) {
      size_t i = rend-rit-1;
      codes[i] = rtn % (*rit);
      rtn = rtn / (*rit);
    }

    return codes;
  }

  uint32_t getMaxIndex() { return maxIdx_; }
  
  ~HGCalDenseIndexerBase() {}

 private:

  void check(size_t osize) {
    if(osize != N)
      throw cms::Exception("ValueError")
        << " unable to update indexer max values. Expected " << N << " received " << osize;
  }

  uint32_t maxIdx_;
  IndexRanges_t vmax_;
  
};


#endif
