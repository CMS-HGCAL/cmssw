#include "DQM/HGCal/interface/CellStatistics.h"
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {

  //initiate stats class
  hgcal::CellStatistics stats;
  assert(stats.n==0);
  assert(stats.sum_x==0);
  assert(stats.sum_xx==0);
  assert(stats.sum_s.size()==0);
  assert(stats.sum_ss.size()==0);
  assert(stats.sum_xs.size()==0);

  //add some events and verify calculations (taken from a random example in web)
  stats.update(34.86,{43.04});
  stats.update(42.58,{51.88});
  stats.update(71.73,{88.55});
  stats.update(110.77,{130.69});
  stats.update(259.95,{314.17});
  std::vector<double> slopes = stats.getSlopes();
  std::vector<double> intercepts = stats.getIntercepts();
  std::vector<double> Rs = stats.getPearsonCorrelation();
  assert(fabs(stats.sum_x-519.89)<0.01);
  assert(fabs(stats.sum_xx-88017.46)<0.01);
  assert(fabs(stats.sum_s[0]-628.33)<0.01); 
  assert(fabs(stats.sum_ss[0]-128167.74)<0.01);
  assert(fabs(stats.sum_xs[0]-106206.14)<0.01);
  assert(fabs(slopes[0]-0.52)<0.01);
  assert(fabs(intercepts[0]-1.20)<0.01);
  assert(fabs(Rs[0]-0.99)<0.01);

  //clear and restart
  stats.clear();
  assert(stats.n==0);
  assert(stats.sum_x==0);
  assert(stats.sum_xx==0);
  assert(stats.sum_s.size()==0);
  assert(stats.sum_ss.size()==0);
  assert(stats.sum_xs.size()==0);

  
  return 0;
}
