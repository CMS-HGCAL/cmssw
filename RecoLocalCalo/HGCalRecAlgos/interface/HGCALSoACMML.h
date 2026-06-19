#ifndef RecoLocalCalo_HGCalRecAlgos_interface_HGCALSoACMML_h
#define RecoLocalCalo_HGCalRecAlgos_interface_HGCALSoACMML_h

#include "DataFormats/SoATemplate/interface/SoACommon.h"
#include "DataFormats/SoATemplate/interface/SoALayout.h"

namespace hgcalcmml {

  // Column order determines DNN input tensor layout (all 21 columns are adjacent float,
  // so TensorCollection can register them as a single contiguous [ndigis,21] tensor).
  //
  // cm0..cm11: pedestal-subtracted CM sum per eRx (digi.cm() of first channel in each eRx),
  //            filled with 0 for eRx slots beyond the module's nErx.
  // unconn0..3: pedestal-subtracted ADC of the 4 unconnected channels in this digi's eRx
  //             (within-eRx positions 8, 17, 19, 28).
  // All 21 float columns must remain adjacent in this definition for TensorCollection
  // contiguity; do not reorder or insert non-float columns between them.
  GENERATE_SOA_LAYOUT(HGCalCMMLSoALayout,
                      SOA_COLUMN(float, cm0),        // CM sum eRx 0
                      SOA_COLUMN(float, cm1),        // CM sum eRx 1
                      SOA_COLUMN(float, cm2),        // CM sum eRx 2
                      SOA_COLUMN(float, cm3),        // CM sum eRx 3
                      SOA_COLUMN(float, cm4),        // CM sum eRx 4
                      SOA_COLUMN(float, cm5),        // CM sum eRx 5
                      SOA_COLUMN(float, cm6),        // CM sum eRx 6
                      SOA_COLUMN(float, cm7),        // CM sum eRx 7
                      SOA_COLUMN(float, cm8),        // CM sum eRx 8
                      SOA_COLUMN(float, cm9),        // CM sum eRx 9
                      SOA_COLUMN(float, cm10),       // CM sum eRx 10
                      SOA_COLUMN(float, cm11),       // CM sum eRx 11
                      SOA_COLUMN(float, msubchidx),  // mean-subtracted channel index, unique per channel
                      SOA_COLUMN(float, msuberxidx), // mean-subtracted ERX index, same within ERX
                      SOA_COLUMN(float, cellfrac),   // cell area fraction (SF key in cell mapping)
                      SOA_COLUMN(float, unconn0),    // ADC of unconnected channel at eRx pos 8
                      SOA_COLUMN(float, unconn1),    // ADC of unconnected channel at eRx pos 17
                      SOA_COLUMN(float, unconn2),    // ADC of unconnected channel at eRx pos 19
                      SOA_COLUMN(float, unconn3),    // ADC of unconnected channel at eRx pos 28
                      SOA_COLUMN(float, ntoa),       // event-level count of digis with TOA > 0
                      SOA_COLUMN(float, ntot))       // event-level count of digis with TOT > 0

  using HGCalCMMLSoA = HGCalCMMLSoALayout<>;

}  // namespace hgcalcmml

#endif  // RecoLocalCalo_HGCalRecAlgos_interface_HGCALSoACMML_h
