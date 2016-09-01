#ifndef RecoHGCAL_HGCALClusters_HGCalImagingAlgo_h
#define RecoHGCAL_HGCALClusters_HGCalImagingAlgo_h

#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "Geometry/CaloTopology/interface/HGCalTopology.h"
#include "DataFormats/ForwardDetId/interface/HGCEEDetId.h"
#include "DataFormats/HGCRecHit/interface/HGCRecHitCollections.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometry.h"
#include "Geometry/CaloGeometry/interface/TruncatedPyramid.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "FWCore/Framework/interface/Event.h"

#include "DataFormats/Math/interface/Point3D.h"
#include "DataFormats/EgammaReco/interface/BasicCluster.h"

//#include "RecoLocalCalo/HGCalRecHitDump/interface/RecHitTools.h"

// C/C++ headers
#include <string>
#include <vector>
#include <set>

#include "KDTreeLinkerAlgoT.h"

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

 HGCalImagingAlgo() : layer_select(-1), delta_c(0.), kappa(1.), ecut(0.),
                      cluster_offset(0),
		      geometry(0), ddd(0),
		      //topology(*thetopology_p),
		      algoId(reco::CaloCluster::undefined),
		      verbosity(pERROR),
		      eventsToDisplay(0){
 }

  HGCalImagingAlgo(float delta_c_in, double kappa_in, double ecut_in,
		   const HGCalGeometry *thegeometry_p,
		   //		   const CaloSubdetectorTopology *thetopology_p,
		   reco::CaloCluster::AlgoId algoId_in,
		   VerbosityLevel the_verbosity = pERROR,
           int dbglayer = -1,
           unsigned int theEventsToDisplay = 0) :
                              layer_select(dbglayer),
                              delta_c(delta_c_in),
                              kappa(kappa_in),
						      ecut(ecut_in),
						      cluster_offset(0),
						      sigma2(1.0),
						      geometry(thegeometry_p),
						      //topology(*thetopology_p),
						      algoId(algoId_in),
						      verbosity(the_verbosity),
						      eventsToDisplay(theEventsToDisplay){
  }


  HGCalImagingAlgo(float delta_c_in, double kappa_in, double ecut_in,
		   double showerSigma,
		   const HGCalGeometry *thegeometry_p,
		   //		   const CaloSubdetectorTopology *thetopology_p,
		   reco::CaloCluster::AlgoId algoId_in,
		   VerbosityLevel the_verbosity = pERROR,
           int dbglayer = -1,
           unsigned int theEventsToDisplay = 0) :
                              layer_select(dbglayer),
                              delta_c(delta_c_in),
                              kappa(kappa_in),
							  ecut(ecut_in),
							  cluster_offset(0),
							  sigma2(std::pow(showerSigma,2.0)),
							  geometry(thegeometry_p),
							  //topology(*thetopology_p),
							  algoId(algoId_in),
							  verbosity(the_verbosity),
							  eventsToDisplay(theEventsToDisplay){
  }


  virtual ~HGCalImagingAlgo()
    {
    }

  void setVerbosity(VerbosityLevel the_verbosity)
    {
      verbosity = the_verbosity;
    }

  // this is the method that will start the clusterisation (it is possible to invoke this method more than once - but make sure it is with
  // different hit collections (or else use reset)
  void makeClusters(const HGCRecHitCollection &hits);
  // this is the method to get the cluster collection out
  std::vector<reco::BasicCluster> getClusters(bool);
  // needed to switch between EE and HE with the same algorithm object (to get a single cluster collection)
  void setGeometry(const HGCalGeometry *thegeometry_p){geometry = thegeometry_p;}
  // use this if you want to reuse the same cluster object but don't want to accumulate clusters (hardly useful?)
  void reset(){
    current_v.clear();
    clusters_v.clear();
    cluster_offset = 0;
  }
  // @@EM ToDo: debugging utilities (to be removed in the future)
  void dumpToDisplayMaybe(const edm::Event&);
  /// point in the space
  typedef math::XYZPoint Point;

 private:

  //max number of layers
  static const unsigned int maxlayer = 39;
  const int layer_select; // for debugging

  // The two parameters used to identify clusters
  float delta_c;
  double kappa;

  // The hit energy cutoff
  double ecut;

  // The current offset into the temporary cluster structure
  unsigned int cluster_offset;

  // for energy sharing
  double sigma2; // transverse shower size

  // The vector of clusters
  std::vector<reco::BasicCluster> clusters_v;

  const HGCalGeometry *geometry;
  //  const HGCalTopology &topology;
  const HGCalDDDConstants* ddd;

  // The algo id
  reco::CaloCluster::AlgoId algoId;

  // The verbosity level
  VerbosityLevel verbosity;

  // Number of events to be dumped for standalone display
  unsigned int eventsToDisplay;

  struct Hexel {

    double x;
    double y;
    double z;
    bool isHalfCell;
    double weight;
    double fraction;
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
      weight(0.), fraction(1.0), detid(id_in), rho(0.), delta(0.),
      nearestHigher(-1), isBorder(false), isHalo(false),
      clusterIndex(-1), geometry(geometry_in)
    {
      const GlobalPoint position( std::move( geometry->getPosition( detid ) ) );
      const HGCalGeometry::CornersVec corners( std::move( geometry->getCorners( detid ) ) );

      weight = hit.energy();
      x = position.x();
      y = position.y();
      z = position.z();

    }
    Hexel() :
      x(0.),y(0.),z(0.),isHalfCell(false),
      weight(0.), fraction(1.0), detid(), rho(0.), delta(0.),
      nearestHigher(-1), isBorder(false), isHalo(false),
      clusterIndex(-1),
      geometry(0)
    {}
    bool operator > (const Hexel& rhs) const {
      return (rho > rhs.rho);
    }

  };

  typedef KDTreeLinkerAlgo<Hexel,2> KDTree;
  typedef KDTreeNodeInfoT<Hexel,2> KDNode;


  // A vector of vectors of KDNodes holding an Hexel in the clusters - to be used to build CaloClusters of DetIds
  std::vector< std::vector<KDNode> > current_v;

  std::vector<size_t> sort_by_delta(const std::vector<KDNode> &v){
    std::vector<size_t> idx(v.size());
    for (size_t i = 0; i != idx.size(); ++i) idx[i] = i;
    sort(idx.begin(), idx.end(),
	 [&v](size_t i1, size_t i2) {return v[i1].data.delta > v[i2].data.delta;});
    return idx;
  }

  std::vector<std::vector<Hexel> > points; //a vector of vectors of hexels, one for each layer
  //@@EM todo: the number of layers should be obtained programmatically - the range is 1-n instead of 0-n-1...

  //these functions should be in a helper class.
  double distance(const Hexel &pt1, const Hexel &pt2); //2-d distance on the layer (x-y)
  double calculateLocalDensity(std::vector<KDNode> &, KDTree &); //return max density
  double calculateDistanceToHigher(std::vector<KDNode> &, KDTree &);
  int findAndAssignClusters(std::vector<KDNode> &, KDTree &, double, KDTreeBox &);
  math::XYZPoint calculatePosition(std::vector<KDNode> &);

  // attempt to find subclusters within a given set of hexels
  std::vector<unsigned> findLocalMaximaInCluster(const std::vector<KDNode>&);
  math::XYZPoint calculatePositionWithFraction(const std::vector<KDNode>&, const std::vector<double>&);
  double calculateEnergyWithFraction(const std::vector<KDNode>&, const std::vector<double>&);
  // outputs
  void shareEnergy(const std::vector<KDNode>&,
		   const std::vector<unsigned>&,
		   std::vector<std::vector<double> >&);


  class DumpToDisplay{

  public:

    struct hit{
      uint32_t id;
      float energy;
      float density;
      float distance;
      uint32_t cindex;
      uint32_t flags;
      bool isBorder()const {return (bool)flags&0x1;}
      bool isHalo()const {return (bool)flags&0x2;}
    };
    std::string getFileName(const edm::Event& iEvent){
      char filename[50];
      sprintf(filename,"run%06dev%010llu.dat",iEvent.run(),iEvent.id().event());
      std::string fileName(filename);
      std::cout << "creating file " << fileName << std::endl;
      return fileName;
    }
    void writeAll(std::vector< std::vector<HGCalImagingAlgo::KDNode> >&hits, std::string fileName){
      FILE *file = fopen(fileName.c_str(),"w+");
      for (unsigned int i = 0; i < hits.size(); i++){
	std::vector< HGCalImagingAlgo::KDNode >::iterator it;
	for (it = hits[i].begin(); it != hits[i].end(); it++){
	  hit h;
	  h.id         = (*it).data.detid;
	  h.energy     = (*it).data.weight;
	  h.density    = (*it).data.rho;
	h.distance   = (*it).data.delta;
	h.cindex     = i;
	h.flags      = (uint32_t)(*it).data.isBorder | ((uint32_t)(*it).data.isHalo << 1);
	fwrite(&h,sizeof(hit),1,file);
	std::cout << h.id << " " << h.energy << " " << h.density << " " << h.distance << " " << h.cindex
		  << " " << h.flags << " " << (*it).data.isBorder << " " << (*it).data.isHalo << std::endl;
	}
      }
      fclose(file);
    }
  };
  // Utilities to dump internal hit structure for event display
  DumpToDisplay dumper;


 };

#endif
