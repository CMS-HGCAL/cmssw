#ifndef RecoLocalCalo_HGCalRecAlgos_interface_HGCALSoACMML_h
#define RecoLocalCalo_HGCalRecAlgos_interface_HGCALSoACMML_h
#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"

namespace hgcalcmml {
  GENERATE_SOA_LAYOUT(HGCalCMMLSoALayout,
                      SOA_COLUMN(float, cm0),        // [ 0] pedestal-subtracted CM average eRx 0:  0.5*digi.cm() - CM_ped
                      SOA_COLUMN(float, cm1),        // [ 1] pedestal-subtracted CM average eRx 1:  0.5*digi.cm() - CM_ped
                      SOA_COLUMN(float, cm2),        // [ 2] pedestal-subtracted CM average eRx 2:  0.5*digi.cm() - CM_ped
                      SOA_COLUMN(float, cm3),        // [ 3] pedestal-subtracted CM average eRx 3:  0.5*digi.cm() - CM_ped
                      SOA_COLUMN(float, cm4),        // [ 4] pedestal-subtracted CM average eRx 4:  0.5*digi.cm() - CM_ped
                      SOA_COLUMN(float, cm5),        // [ 5] pedestal-subtracted CM average eRx 5:  0.5*digi.cm() - CM_ped
                      SOA_COLUMN(float, cm6),        // [ 6] pedestal-subtracted CM average eRx 6:  0.0 if eRx inactive
                      SOA_COLUMN(float, cm7),        // [ 7] pedestal-subtracted CM average eRx 7:  0.0 if eRx inactive
                      SOA_COLUMN(float, cm8),        // [ 8] pedestal-subtracted CM average eRx 8:  0.0 if eRx inactive
                      SOA_COLUMN(float, cm9),        // [ 9] pedestal-subtracted CM average eRx 9:  0.0 if eRx inactive
                      SOA_COLUMN(float, cm10),       // [10] pedestal-subtracted CM average eRx 10: 0.0 if eRx inactive
                      SOA_COLUMN(float, cm11),       // [11] pedestal-subtracted CM average eRx 11: 0.0 if eRx inactive
                      SOA_COLUMN(float, ntoa),       // [12] nchtoa: module-level count of digis with TOA > 0
                      SOA_COLUMN(float, ntot),       // [13] nchtot: module-level count of digis with TOT > 0
                      SOA_COLUMN(float, msubchidx),  // [14] channel_indices: mean-subtracted channel index, unique per channel, float(chIdx) - (float(nErx) * 37.0f - 1.0f) / 2.0f;
                      SOA_COLUMN(float, msuberxidx), // [15] erx_indices: mean-subtracted ERX index, same within ERX, float(erxIdx) - (float(nErx) - 1.0f) / 2.0f;
                      SOA_COLUMN(float, cellfrac),   // [16] cell_area_fraction: ML_F.SF or MH_F.SF from cellareas.json, indexed by chIdx
                      SOA_COLUMN(float, unconn0),    // [17] pedestal-subtracted ADC of unconnected channel at eRx pos 8
                      SOA_COLUMN(float, unconn1),    // [18] pedestal-subtracted ADC of unconnected channel at eRx pos 17
                      SOA_COLUMN(float, unconn2),    // [19] pedestal-subtracted ADC of unconnected channel at eRx pos 19
                      SOA_COLUMN(float, unconn3))    // [20] pedestal-subtracted ADC of unconnected channel at eRx pos 28

  using HGCalCMMLSoA = HGCalCMMLSoALayout<>;

}

#endif
