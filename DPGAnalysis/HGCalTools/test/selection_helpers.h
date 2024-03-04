#ifndef _selection_helpers_h_
#define _selection_helpers_h_

#include <iostream>
#include "ROOT/RVec.hxx"
#include "TMath.h"

using rvec_f = const ROOT::VecOps::RVec<float>;
using rvec_s = const ROOT::VecOps::RVec<Short_t>;
using rvec_us = const ROOT::VecOps::RVec<UShort_t>;
using rvec_uc = const ROOT::VecOps::RVec<UChar_t>;
using rvec_ui = const ROOT::VecOps::RVec<UInt_t>;
using rvec_i = const ROOT::VecOps::RVec<Int_t>;

/**
   @short returns a dense index for a given e-Rx
 */
uint32_t erxDenseIndex(uint16_t slink, uint16_t captureblock, uint16_t econdidx, uint16_t erxid,
                       uint16_t maxCB=10, uint16_t maxEcon=12, uint16_t maxErx=12) {
  uint32_t rtn=slink * maxCB + captureblock;
  rtn = rtn * maxEcon + econdidx;
  rtn = rtn * maxErx + erxid;
  return rtn;
}

/**
   @short returns the max indexfor the system
 */
uint32_t maxErxDenseIndex(uint16_t maxSlink=2, uint16_t maxCB=10,uint16_t maxEcon=12,uint16_t maxErx=12) {
  return erxDenseIndex(maxSlink-1,maxCB-1,maxEcon-1,maxErx-1,maxCB,maxEcon,maxErx);
}

/**
   @short returns the dense index 
*/
rvec_ui fillErxDenseIndex(uint16_t maxSlink=2, uint16_t maxCB=10,uint16_t maxEcon=12,uint16_t maxErx=12) {
  uint32_t max_index = maxErxDenseIndex(maxSlink,maxCB,maxEcon,maxErx);
  std::vector<UInt_t> v(max_index+1);
  std::iota(v.begin(), v.end(), 0);
  return rvec_ui(v.begin(),v.end());
}

/**
    @short build the CM estimator from unconnected channels in the same half
    it assumes channels are ordered by elecid and thus starts a new group when the half roc value changes
*/

rvec_f cmFromChannelList(const rvec_ui&eleid, const rvec_us &adc,uint16_t maxSlink=2, uint16_t maxCB=10,uint16_t maxEcon=12,uint16_t maxErx=12) {

  //initiate the vectors
  uint32_t max_index = maxErxDenseIndex(maxSlink,maxCB,maxEcon,maxErx);
  std::vector<float> cm(max_index+1,0.f), ncts(max_index+1,0.f);

  //loop over channels
  for(size_t i=0; i<adc.size(); i++) {

    uint16_t slink=((eleid[i]>>18) & 0x3ff);
    uint16_t cb=((eleid[i]>>14)&0xf);
    uint16_t econ=((eleid[i]>>10)&0xf);
    uint16_t erx=((eleid[i]>>6)&0xf);
    uint32_t idx = erxDenseIndex(slink,cb,econ,erx,maxCB,maxEcon,maxErx);
    cm[idx] += float(adc[i]);
    ncts[idx] += 1.;
  }

  //finalize average
  for(UInt_t i=0; i<max_index; i++) {
    if(ncts[i]==0) continue;
    cm[i] /= ncts[i];
  }

  return rvec_f(cm.begin(), cm.end());
}


/**
   @short test dense indexing
 */
void _test_selection_helpers() {

  uint16_t maxSlink=2;
  uint16_t maxCB=10;
  uint16_t maxEcon=12;
  uint16_t maxErx=12;
  uint32_t max_index = maxErxDenseIndex(maxSlink,maxCB,maxEcon,maxErx);
  rvec_ui vDenseIndex=fillErxDenseIndex(maxSlink,maxCB,maxEcon,maxErx=12);

  uint32_t idx=0;
  for(uint16_t slink=0; slink<maxSlink; slink++)
    for(uint16_t cb=0; cb<maxCB; cb++)
      for(uint16_t econ=0; econ<maxEcon; econ++)
        for(uint16_t erx=0; erx<maxErx; erx++){
          uint32_t dense_index=erxDenseIndex(slink,cb,econ,erx,maxCB,maxEcon,maxErx);
          assert(dense_index==vDenseIndex[idx]);
          idx++;
        }

  std::cout << "All dense indices in sequence, matching index vector" << std::endl;
}

/**
   @short returns the CM given an ECON/eRx pair
*/
uint16_t getCMForChannel(uint16_t econd, uint16_t erx,
                      const rvec_us&cm_econdIdx, const rvec_us&cm_erx, const rvec_us &adc) {

  //loop over channels
  for(size_t i=0; i<adc.size(); i++) {

    if(cm_econdIdx[i]!=econd) continue;
    if(cm_erx[i]!=erx) continue;

    return adc[i];
  }

  return 0;
}


/**
   @short returns the CM given an ECON/eRx pair
*/
uint16_t getUnconnCMForChannel(uint16_t econdIdx, uint16_t erx,
                               const rvec_us&ch_econdIdx, const rvec_us&ch_erx, const rvec_us &ch_adc, const rvec_i &ch_type) {

  //loop over channels
  float sum(0.),n(0.);
  for(size_t i=0; i<ch_adc.size(); i++) {

    if(ch_econdIdx[i]!=econdIdx) continue;
    if(ch_erx[i]!=erx) continue;
    if(ch_type[i]!=-1) continue;
    n++;
    sum+=ch_adc[i];
  }

  return n==0 ? 0 : sum/n;
}


#endif
