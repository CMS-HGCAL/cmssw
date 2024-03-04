#ifndef __dqm_hgcal_interface_runningstatistics_h_
#define __dqm_hgcal_interface_runningstatistics_h_

#include <stdio.h>
#include <cmath>
#include <algorithm>

namespace hgcal {

  class CellStatistics {

  public:
    /**
       @short default constructor
    */
    CellStatistics() {
      this->clear();
    }
    
    /**
       @short copy from another class
    */
    CellStatistics(CellStatistics &t)
    {
      n=t.n;
      sum_x=t.sum_x;
      sum_xx=t.sum_xx;
      sum_s=t.sum_s;
      sum_ss=t.sum_ss;
      sum_xs=t.sum_xs;
    }
    
    /**
       @short clears sums
    */
    void clear() {
      n=0;
      sum_x=0;
      sum_xx=0;
      sum_s.clear();
      sum_ss.clear();
      sum_xs.clear();
    }
    
    /*
      @short updates the sums for the observable x and the spectators
    */
    void update(double x, std::vector<double> s) {
      
      n += 1;

      //resize sums if needed
      size_t nspecs(s.size());
      if(n==1) {
        sum_s.resize(nspecs,0);
        sum_ss.resize(nspecs,0);
        sum_xs.resize(nspecs,0);
      }

      //update sums
      sum_x += x;
      sum_xx += pow(x,2);
      for(size_t i=0; i<nspecs; i++) {
        sum_s[i] += s[i];
        sum_ss[i] += pow(s[i],2);
        sum_xs[i] += s[i]*x;
      }     
    }
    
    /**
       @short returns a par <mean,variance> given a sum of values, values^2 and counts
    */
    std::pair<double,double> getStats(double x,double xx,double ncts) {
    
      double avg(0),var(0);
      if(ncts>0) {
        avg = x/ncts;
        if(ncts>1) {
          var = (ncts/(ncts-1))*(xx/ncts-avg*avg);
        }
      }

      double sigma = (var<0 ? -1 : 1)*sqrt(fabs(var));
      return std::pair<double,double>(avg,sigma);
    }

    /**
       @short mean and variance for the observable
    */
    std::pair<double,double> getObservableStats() { return getStats(sum_x,sum_xx,n); }

    /**
       @short mean and variance for a spectator
    */
    std::pair<double,double> getSpectatorStats(size_t i) {
      std::pair<double,double> obs_stats(0,0);
      if(i<sum_s.size()) obs_stats = getStats(sum_s[i],sum_ss[i],n);
      return obs_stats;
    }
    
    /**
       @short returns the slope estimate for a linear relation between the observable and the spectators
    */
    std::vector<double> getSlopes() {

      std::vector<double> slopes(sum_s.size(),0);
      for(size_t i=0; i<slopes.size(); i++) {
        double den = (n*sum_xx-pow(sum_x,2));
        if(den==0) continue;
        slopes[i] = (sum_s[i]*sum_xx-sum_x*sum_xs[i]) / den;
      }
      return slopes;
    }

    /**
       @short returns the intercept estimate for a linear relation between the observable and the spectators
    */
    std::vector<double> getIntercepts() {

      std::vector<double> intercepts(sum_s.size(),0);
      for(size_t i=0; i<intercepts.size(); i++) {
        double den = (n*sum_xx-pow(sum_x,2) );
        if(den==0) continue;
        intercepts[i] = (n*sum_xs[i]-sum_x*sum_s[i]) / den;
      }
      return intercepts;
    }

    /**
       @short Pearson correlation factor
    */
    std::vector<double> getPearsonCorrelation() {
      std::vector<double> r(sum_s.size(),0);
      for(size_t i=0; i<r.size(); i++){
        double den = (n*sum_xx - pow(sum_x,2)) * (n*sum_ss[i]-pow(sum_s[i],2));
        if(den<=0) continue;
        r[i] = (n*sum_xs[i]-sum_x*sum_s[i]) / sqrt(den);
      }      
      return r;
    }
 
    /*
      void print_CellStatistics()
      {
      printf("cur_.nevents = %.3f\n"     , cur_.nevents     );
      printf("cur_.mean_x = %.3f\n"      , cur_.mean_x      );
      printf("cur_.mean_y = %.3f\n"      , cur_.mean_y      );
      printf("cur_.variance_x = %.3f\n"  , cur_.variance_x  );
      printf("cur_.variance_y = %.3f\n"  , cur_.variance_y  );
      printf("cur_.covariance = %.3f\n"  , cur_.covariance  );
      printf("cur_.correlation = %.3f\n" , cur_.correlation );
      printf("cur_.slope = %.3f\n"       , cur_.slope       );
      printf("cur_.intercept = %.3f\n"   , cur_.intercept   );
      printf("\n");
      }
    */

    double n,sum_x,sum_xx;
    std::vector<double> sum_s,sum_ss,sum_xs;
 };

}


#endif
