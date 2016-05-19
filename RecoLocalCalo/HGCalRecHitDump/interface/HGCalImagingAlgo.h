#ifndef RecoHGCAL_HGCALClusters_HGCalImagingAlgo_h
#define RecoHGCAL_HGCALClusters_HGCalImagingAlgo_h

#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "DataFormats/ForwardDetId/interface/HGCEEDetId.h"
#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometry.h"
#include "Geometry/CaloGeometry/interface/TruncatedPyramid.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"

#include "DataFormats/Math/interface/Point3D.h"
#include "DataFormats/EgammaReco/interface/BasicCluster.h"

//#include "RecoLocalCalo/HGCalRecHitDump/interface/RecHitTools.h"

// C/C++ headers
#include <string>
#include <vector>
#include <set>


template <typename T>
std::vector<size_t> sorted_indices(const std::vector<T> &v) {
  
  // initialize original index locations
  std::vector<size_t> idx(v.size());
  for (size_t i = 0; i != idx.size(); ++i) idx[i] = i;
  
  // sort indices based on comparing values in v
  sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) {return v[i1] > v[i2];});
  
  return idx;
} 


class HGCalImagingAlgo 
{
 public:
  
  enum VerbosityLevel { pDEBUG = 0, pWARNING = 1, pINFO = 2, pERROR = 3 }; 
  
  HGCalImagingAlgo() : delta_c(0.), kappa(1.), ecut(0.), cluster_offset(0),
		       geometry(0), ddd(0), 
		       //topology(*thetopology_p), 
		       verbosity(pERROR), 
		       points(maxlayer+1){
  }

  HGCalImagingAlgo(double delta_c_in, double kappa_in, double ecut_in,
		   const HGCalGeometry *thegeometry_p,
		   //		   const CaloSubdetectorTopology *thetopology_p,
		   VerbosityLevel the_verbosity = pERROR) : delta_c(delta_c_in), kappa(kappa_in), ecut(ecut_in),
							    cluster_offset(0),
							    geometry(thegeometry_p), 
							    //topology(*thetopology_p), 
							    verbosity(the_verbosity), 
							    points(maxlayer+1){
  }
  
  virtual ~HGCalImagingAlgo()
    {
    }

  void setVerbosity(VerbosityLevel the_verbosity)
    {
      verbosity = the_verbosity;
    }

  // this is the method that will start the clusterisation
  std::vector<reco::BasicCluster> makeClusters(const HGCRecHitCollection &hits);

  /// point in the space
  typedef math::XYZPoint Point;

 private: 
  
  //max number of layers
  static const unsigned int maxlayer = 28;

  // The two parameters used to identify clusters
  double delta_c;
  double kappa;

  // The hit energy cutoff
  double ecut;

  // The current offset into the temporary cluster structure
  unsigned int cluster_offset;


  // The vector of clusters
  std::vector<reco::BasicCluster> clusters_v;

  const HGCalGeometry *geometry;
  //  const HGCalTopology &topology;
  const HGCalDDDConstants* ddd;

  // The verbosity level
  VerbosityLevel verbosity;
  
  struct Hexel {

    double x;
    double y;
    double z;
    bool isHalfCell;
    double weight;
    DetId detid;
    double rho;
    double delta;
    int nearestHigher;
    bool isBorder;
    bool isHalo;
    int clusterIndex;
    const HGCalGeometry *geometry;

    Hexel(const HGCRecHit &hit, DetId id_in, bool isHalf, const HGCalGeometry *geometry_in) : 
      x(0.),y(0.),z(0.),isHalfCell(isHalf),
      weight(0.),detid(id_in), rho(0.), delta(0.),
      nearestHigher(-1), isBorder(false), isHalo(false), 
      clusterIndex(-1), geometry(geometry_in)
    {
      const GlobalPoint position( std::move( geometry->getPosition( detid ) ) );
      const HGCalGeometry::CornersVec corners( std::move( geometry->getCorners( detid ) ) );
      //      float thickness = abs(corners[7].z()-corners[0].z());
      float fudge_thickness = 1.;
      //this is supposed to no longer be necessary...
      // if(thickness<.025 && thickness > .011)
      // 	  fudge_thickness = 2.;
      // 	else if(thickness>.021)
      // 	  fudge_thickness = 2.82;
      weight = hit.energy()/fudge_thickness;
      x = position.x();
      y = position.y();
      z = position.z();
      
    }
    Hexel() : 
      x(0.),y(0.),z(0.),isHalfCell(false),
      weight(0.),detid(), rho(0.), delta(0.),
      nearestHigher(-1), isBorder(false), isHalo(false), 
      clusterIndex(-1),
      geometry(0)
    {}
    bool operator > (const Hexel& rhs) const { 
      return (rho > rhs.rho); 
    }
    
  };
  // A vector of vectors of DetId's in the clusters
  std::vector< std::vector<Hexel> > current_v;

  std::vector<size_t> sort_by_delta(const std::vector<Hexel> &v){
    std::vector<size_t> idx(v.size());
    for (size_t i = 0; i != idx.size(); ++i) idx[i] = i;
    sort(idx.begin(), idx.end(),
	 [&v](size_t i1, size_t i2) {return v[i1].delta > v[i2].delta;});
    return idx;
  }

  std::vector<std::vector<Hexel> > points; //a vector of vectors of hexels, one for each layer
  //@@EM todo: the number of layers should be obtained programmatically - the range is 1-n instead of 0-n-1...

  //these functions should be in a helper class.
  double distance(Hexel &pt1, Hexel &pt2); //2-d distance on the layer (x-y)
  double calculateLocalDensity(std::vector<Hexel> &); //return max density
  double calculateDistanceToHigher(std::vector<Hexel> &);
  int findAndAssignClusters(std::vector<Hexel> &, double);
  math::XYZPoint calculatePosition(std::vector<Hexel> &);
 };

#endif
