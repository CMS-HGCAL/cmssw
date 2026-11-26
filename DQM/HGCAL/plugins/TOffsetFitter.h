#ifndef _toffset_fit_helper_h_
#define _toffset_fit_helper_h_

#include "CondFormats/RunInfo/interface/LHCInfoPerFill.h"
#include <vector>
#include <algorithm>
#include <iostream>

// adapted from Alicia Triana De Las Heras
// original on GitHub: https://gitlab.cern.ch/atriana/timing_jumps_precision/-/blob/master/TOffsetFitter_crate_2.h

class TOffsetFitter {
public:
  explicit TOffsetFitter(size_t ncrates) : ncrates_(ncrates) {}

  /**
        @short sets the new values for the train start edges
    */
  static constexpr auto nbunches_ = LHCInfoPerFill::bunchSlots;
  void setTrainStartEdges(const std::vector<int>& v) { train_start_edges_ = v; }

  /**
        @short implements the prediction of the time offset based on the bunch in which the event has been registered
        Parameters:
            par[0] -> global offset
            par[1] -> layer slope
            par[2 ... ncrates+1] -> crate offsets
            par[ncrates+2] -> train slope
            par[ncrates+3 ... nedges+ncrates+2] -> train offsets
        The prediction is y = global_offset + crate_offset[crate] + train_offset[train] + train_slope * bx + layer_slope * layer
    */
  double operator()(double* x, double* par) {
    const int x_value = x[0];
    // x_value = (layer * ncrates + crate) * nbunches + bunch
    const auto bx = x_value % nbunches_;
    const auto lycrate = x_value / nbunches_;
    const auto crate = lycrate % ncrates_;
    const auto layer = lycrate / ncrates_;

    // Find the first edge strictly greater than bx.
    auto it = std::upper_bound(train_start_edges_.begin(), train_start_edges_.end(), bx);
    auto offset = par[0] + par[crate + 2];  // global_offset + crate_offset
    if (it != train_start_edges_.begin()) {
      const auto idx = std::distance(train_start_edges_.begin(), it) + ncrates_ + 2;
      offset += par[idx];  // + train_offset
    }
    const auto& layer_slope = par[1];
    const auto& train_slope = par[ncrates_ + 2];

    return offset + train_slope * bx + layer_slope * layer;
  }

  /**
        @short debug the train edges
    */
  void print() {
    std::cout << "[TOffsetFitter][print] currently with " << train_start_edges_.size() << " train edges in memory"
              << std::endl;
    for (auto x : train_start_edges_)
      std::cout << x << " ";
    std::cout << std::endl;
  }

  size_t npars() { return ncrates_ + train_start_edges_.size() + 3; }  // ncrates + nedges + 3

private:
  const size_t ncrates_;
  std::vector<int> train_start_edges_;
};

#endif
