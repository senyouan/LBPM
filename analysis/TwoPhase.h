// Header file for two-phase averaging class
#ifndef TwoPhase_INC
#define TwoPhase_INC

#include <vector>

#include "analysis/pmmc.h"
#include "common/Domain.h"
#include "common/Communication.h"
#include "analysis/analysis.h"

#include "shared_ptr.h"
#include "common/Utilities.h"
#include "common/MPI_Helpers.h"
#include "IO/MeshDatabase.h"
#include "IO/Reader.h"
#include "IO/Writer.h"


class TwoPhase{

	//...........................................................................
	int n_nw_pts,n_ns_pts,n_ws_pts,n_nws_pts,n_local_sol_pts,n_local_nws_pts;
	int n_nw_tris,n_ns_tris,n_ws_tris,n_nws_seg,n_local_sol_tris;
	//...........................................................................
	int nc;
	int kstart,kfinish;

	double fluid_isovalue, solid_isovalue;
	double Volume;
	// initialize lists for vertices for surfaces, common line
	DTMutableList<Point> nw_pts;
	DTMutableList<Point> ns_pts;
	DTMutableList<Point> ws_pts;
	DTMutableList<Point> nws_pts;
	DTMutableList<Point> local_sol_pts;
	DTMutableList<Point> local_nws_pts;
	DTMutableList<Point> tmp;

	// initialize triangle lists for surfaces
	IntArray nw_tris;
	IntArray ns_tris;
	IntArray ws_tris;
	IntArray nws_seg;
	IntArray local_sol_tris;

	// Temporary storage arrays
	DoubleArray CubeValues;
	DoubleArray Values;
	DoubleArray DistanceValues;
	DoubleArray KGwns_values;
	DoubleArray KNwns_values;
	DoubleArray InterfaceSpeed;
	DoubleArray NormalVector;

	DoubleArray RecvBuffer;

	char *TempID;

	// CSV / text file where time history of averages is saved
	FILE *TIMELOG;
	FILE *NWPLOG;
	FILE *WPLOG;

public:
	//...........................................................................
	Domain& Dm;
	int NumberComponents_WP,NumberComponents_NWP;
	//...........................................................................
	// Averaging variables
	//...........................................................................
	// local averages (to each MPI process)
	double trimdist; 						// pixel distance to trim surface for specified averages
	double porosity,poreVol;
	double awn,ans,aws,lwns;
	double wp_volume,nwp_volume;
	double As, dummy;
	double vol_w, vol_n;						// volumes the exclude the interfacial region
	double sat_w, sat_w_previous;
	double pan,paw;								// local phase averaged pressure
	// Global averages (all processes)
	double pan_global,paw_global;				// local phase averaged pressure
	double vol_w_global, vol_n_global;			// volumes the exclude the interfacial region
	double awn_global,ans_global,aws_global;
	double lwns_global;
	double efawns,efawns_global;				// averaged contact angle
	double euler,Kn,Jn,An;
	double euler_global,Kn_global,Jn_global,An_global;

	double Jwn,Jwn_global;						// average mean curavture - wn interface
	double Kwn,Kwn_global;						// average Gaussian curavture - wn interface
	double KNwns,KNwns_global;					// wns common curve normal curavture
	double KGwns,KGwns_global;					// wns common curve geodesic curavture
	double trawn,trawn_global;					// trimmed interfacial area
	double trJwn,trJwn_global;					// trimmed interfacial area
	double trRwn,trRwn_global;					// trimmed interfacial area
	double nwp_volume_global;					// volume for the non-wetting phase
	double wp_volume_global;					// volume for the wetting phase
	double As_global;
	double wwndnw, wwndnw_global;
	double wwnsdnwn, wwnsdnwn_global;
	double Jwnwwndnw, Jwnwwndnw_global;
	double dEs,dAwn,dAns;						// Global surface energy (calculated by rank=0)
	DoubleArray van;
	DoubleArray vaw;
	DoubleArray vawn;
	DoubleArray vawns;
	DoubleArray Gwn;
	DoubleArray Gns;
	DoubleArray Gws;
	DoubleArray van_global;
	DoubleArray vaw_global;
	DoubleArray vawn_global;
	DoubleArray vawns_global;
	DoubleArray Gwn_global;
	DoubleArray Gns_global;
	DoubleArray Gws_global;
	//...........................................................................
	//...........................................................................
        int Nx,Ny,Nz;
	IntArray PhaseID;	// Phase ID array (solid=0, non-wetting=1, wetting=2)
	BlobIDArray Label_WP;   // Wetting phase label
	BlobIDArray Label_NWP;  // Non-wetting phase label index (0:nblobs-1)
	std::vector<BlobIDType> Label_NWP_map;  // Non-wetting phase label for each index
	DoubleArray SDn;
	DoubleArray SDs;
	DoubleArray Phase;
	DoubleArray Press;
	DoubleArray dPdt;
	DoubleArray MeanCurvature;
	DoubleArray GaussCurvature;
	DoubleArray SDs_x;		// Gradient of the signed distance
	DoubleArray SDs_y;
	DoubleArray SDs_z;
	DoubleArray SDn_x;		// Gradient of the signed distance
	DoubleArray SDn_y;
	DoubleArray SDn_z;
	DoubleArray DelPhi;		// Magnitude of Gradient of the phase indicator field
	DoubleArray Phase_tplus;
	DoubleArray Phase_tminus;
	DoubleArray Vel_x;		// Velocity
	DoubleArray Vel_y;
	DoubleArray Vel_z;
	//	Container for averages;
	DoubleArray ComponentAverages_WP;
	DoubleArray ComponentAverages_NWP;
	//...........................................................................
	TwoPhase(Domain &dm);
	~TwoPhase();
	void Initialize();
//	void SetupCubes(Domain &Dm);
	void UpdateMeshValues();
	void UpdateSolid();
	void ComputeDelPhi();
	void ColorToSignedDistance(double Beta, DoubleArray &ColorData, DoubleArray &DistData);
	void ComputeLocal();
	void AssignComponentLabels();
	void ComponentAverages();
	void Reduce();
	void WriteSurfaces(int logcount);
	void NonDimensionalize(double D, double viscosity, double IFT);
	void PrintAll(int timestep);
	int GetCubeLabel(int i, int j, int k, IntArray &BlobLabel);
	void SortBlobs();
	void PrintComponents(int timestep);
};


#endif
