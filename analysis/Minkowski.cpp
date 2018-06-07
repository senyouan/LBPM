#include "analysis/Minkowski.h"
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

#define PI 3.14159265359

// Constructor
Minkowski::Minkowski(std::shared_ptr <Domain> dm):
	n_obj_pts(0), n_ns_pts(0), n_ws_pts(0), n_nws_pts(0), n_local_sol_pts(0), n_local_nws_pts(0),
    n_obj_tris(0), n_ns_tris(0), n_ws_tris(0), n_nws_seg(0), n_local_sol_tris(0),
    nc(0), kstart(0), kfinish(0), fluid_isovalue(0), solid_isovalue(0),	Volume(0),
    TIMELOG(NULL), NWPLOG(NULL), WPLOG(NULL), 
    Dm(dm), nwp_volume(0), vol_n_global(0), nwp_volume_global(0)
{
	Nx=dm->Nx; Ny=dm->Ny; Nz=dm->Nz;
	Volume=(Nx-2)*(Ny-2)*(Nz-2)*Dm->nprocx()*Dm->nprocy()*Dm->nprocz()*1.0;

	TempID = new char[Nx*Ny*Nz];

	// Global arrays
	PhaseID.resize(Nx,Ny,Nz);       PhaseID.fill(0);
	SDn.resize(Nx,Ny,Nz);           SDn.fill(0);
	MeanCurvature.resize(Nx,Ny,Nz); MeanCurvature.fill(0);
	GaussCurvature.resize(Nx,Ny,Nz); GaussCurvature.fill(0);
	SDn_x.resize(Nx,Ny,Nz);         SDn_x.fill(0);      // Gradient of the signed distance
	SDn_y.resize(Nx,Ny,Nz);         SDn_y.fill(0);
	SDn_z.resize(Nx,Ny,Nz);         SDn_z.fill(0);
	//.........................................
	// Allocate cube storage space
	CubeValues.resize(2,2,2);
	obj_tris.resize(3,20);
	obj_pts=DTMutableList<Point>(20);
	tmp=DTMutableList<Point>(20);
	//.........................................
	Values.resize(20);
	DistanceValues.resize(20);
	NormalVector.resize(60);
	
	if (Dm->rank()==0){
		TIMELOG = fopen("minkowski.csv","a+");
		if (fseek(TIMELOG,0,SEEK_SET) == fseek(TIMELOG,0,SEEK_CUR))
		{
			// If timelog is empty, write a short header to list the averages
			//fprintf(TIMELOG,"--------------------------------------------------------------------------------------\n");
			fprintf(TIMELOG,"Euler Kn Jn An\n"); 			//miknowski measures,
		}
	}
}


// Destructor
Minkowski::~Minkowski()
{
    if ( TIMELOG!=NULL ) { fclose(TIMELOG); }
}


void Minkowski::Initialize()
{
	fluid_isovalue=0.0;
	vol_n = euler = Jn = An = Kn = 0.0;
}


void Minkowski::UpdateMeshValues()
{
	int i,j,k,n;
	//...........................................................................
	Dm->CommunicateMeshHalo(SDn);
	//...........................................................................
	// Compute the gradients of the phase indicator and signed distance fields
	pmmc_MeshGradient(SDn,SDn_x,SDn_y,SDn_z,Nx,Ny,Nz);
	//...........................................................................
	// Gradient of the phase indicator field
	//...........................................................................
	Dm->CommunicateMeshHalo(SDn_x);
	//...........................................................................
	Dm->CommunicateMeshHalo(SDn_y);
	//...........................................................................
	Dm->CommunicateMeshHalo(SDn_z);
	//...........................................................................
	//...........................................................................
	// Compute the mesh curvature of the phase indicator field
	pmmc_MeshCurvature(SDn, MeanCurvature, GaussCurvature, Nx, Ny, Nz);
	//...........................................................................
	//...........................................................................
	Dm->CommunicateMeshHalo(MeanCurvature);
	//...........................................................................
	Dm->CommunicateMeshHalo(GaussCurvature);
	//...........................................................................
	// Initializing the blob ID
	for (k=0; k<Nz; k++){
		for (j=0; j<Ny; j++){
			for (i=0; i<Nx; i++){
				n = k*Nx*Ny+j*Nx+i;
				if (Dm->id[n] == 0){
					// Solid phase
					PhaseID(i,j,k) = 0;
				}
				else {
					// non-wetting phase
					PhaseID(i,j,k) = 1;
				}
			}
		}
	}

}
void Minkowski::ComputeLocal()
{
	int i,j,k,n,kmin,kmax;
	int cube[8][3] = {{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};

	// If external boundary conditions are set, do not average over the inlet
	kmin=1; kmax=Nz-1;
	if (Dm->BoundaryCondition > 0 && Dm->kproc() == 0) kmin=4;
	if (Dm->BoundaryCondition > 0 && Dm->kproc() == Dm->nprocz()-1) kmax=Nz-4;

	for (k=kmin; k<kmax; k++){
		for (j=1; j<Ny-1; j++){
			for (i=1; i<Nx-1; i++){
				//...........................................................................
				n_obj_pts=0;
				n_obj_tris=0;
				//...........................................................................
				// Compute volume averages
				for (int p=0;p<8;p++){
					n = i+cube[p][0] + (j+cube[p][1])*Nx + (k+cube[p][2])*Nx*Ny;
					if ( Dm->id[n] != 0 ){
						// 1-D index for this cube corner
						if ( Phase(i+cube[p][0],j+cube[p][1],k+cube[p][2]) > 0 ){
							vol_n += 0.125;
						}
					}
				}

				n_obj_pts=n_obj_tris=0;
				// Compute the non-wetting phase surface and associated area
				An += geomavg_MarchingCubes(SDn,fluid_isovalue,i,j,k,obj_pts,n_obj_pts,obj_tris,n_obj_tris);

				Jn += pmmc_CubeSurfaceInterpValue(CubeValues,MeanCurvature,obj_pts,obj_tris,Values,
										i,j,k,n_obj_pts,n_obj_tris);
				// Compute Euler characteristic from integral of gaussian curvature
				Kn += pmmc_CubeSurfaceInterpValue(CubeValues,GaussCurvature,obj_pts,obj_tris,Values,
						i,j,k,n_obj_pts,n_obj_tris);

				euler += geomavg_EulerCharacteristic(obj_pts,obj_tris,n_obj_pts,n_obj_tris,i,j,k);

			}
		}
	}
	
}


void Minkowski::AssignComponentLabels()
{
	/*
	int LabelNWP=1;
	int LabelWP=2;
	// NOTE: labeling the wetting phase components is tricky! One sandstone media had over 800,000 components
	// NumberComponents_WP = ComputeGlobalPhaseComponent(Dm->Nx-2,Dm->Ny-2,Dm->Nz-2,Dm->rank_info,PhaseID,LabelWP,Label_WP);
	// treat all wetting phase is connected
	NumberComponents_WP=1;
	for (int k=0; k<Nz; k++){
		for (int j=0; j<Ny; j++){
			for (int i=0; i<Nx; i++){
				Label_WP(i,j,k) = 0;
				//if (SDs(i,j,k) > 0.0) PhaseID(i,j,k) = 0;
				//else if (Phase(i,j,k) > 0.0) PhaseID(i,j,k) = LabelNWP;
				//else PhaseID(i,j,k) = LabelWP;
			}
		}
	}

	// Fewer non-wetting phase features are present
	//NumberComponents_NWP = ComputeGlobalPhaseComponent(Dm->Nx-2,Dm->Ny-2,Dm->Nz-2,Dm->rank_info,PhaseID,LabelNWP,Label_NWP);
	NumberComponents_NWP = ComputeGlobalBlobIDs(Dm->Nx-2,Dm->Ny-2,Dm->Nz-2,Dm->rank_info,SDs,SDn,solid_isovalue,fluid_isovalue,Label_NWP,Dm->Comm);
	*/
}

void Minkowski::Reduce()
{
	int i;
	double iVol_global=1.0/Volume;
	//...........................................................................
	MPI_Barrier(Dm->Comm);
	// Phase averages
	MPI_Allreduce(&vol_n,&vol_n_global,1,MPI_DOUBLE,MPI_SUM,Dm->Comm);
	MPI_Allreduce(&euler,&euler_global,1,MPI_DOUBLE,MPI_SUM,Dm->Comm);
	MPI_Allreduce(&An,&An_global,1,MPI_DOUBLE,MPI_SUM,Dm->Comm);
	MPI_Allreduce(&Jn,&Jn_global,1,MPI_DOUBLE,MPI_SUM,Dm->Comm);

	MPI_Barrier(Dm->Comm);

	// normalize to per unit volume basis
	vol_n_global /=Volume;
	An_global /=Volume;
	Jn_global /=Volume;
	euler_global /=Volume;
	euler_global /= (2*PI);

}

void Minkowski::NonDimensionalize(double D)
{
	An_global *= D;
	Jn_global *= D*D;
	euler_global *= D*D*D;
}

void Minkowski::PrintAll()
{
	if (Dm->rank()==0){
		fprintf(TIMELOG,"%.5g %.5g %.5g %.5g\n",vol_n_global, An_global, Jn_global, euler_global);			// minkowski measures
		fflush(TIMELOG);
	}


}
