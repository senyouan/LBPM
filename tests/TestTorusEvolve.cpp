// Sequential blob analysis 
// Reads parallel simulation data and performs connectivity analysis
// and averaging on a blob-by-blob basis
// James E. McClure 2014

#include <iostream>
#include <math.h>
#include "common/Communication.h"
#include "analysis/analysis.h"
#include "analysis/Minkowski.h"
#include "IO/MeshDatabase.h"

std::shared_ptr<Database> loadInputs( int nprocs )
{
  //auto db = std::make_shared<Database>( "Domain.in" );
    auto db = std::make_shared<Database>();
    db->putScalar<int>( "BC", 0 );
    db->putVector<int>( "nproc", { 1, 1, 1 } );
    db->putVector<int>( "n", { 100, 100, 100 } );
    db->putScalar<int>( "nspheres", 1 );
    db->putVector<double>( "L", { 1, 1, 1 } );
    return db;
}


int main(int argc, char **argv)
{
  // Initialize MPI
  int rank, nprocs;
  MPI_Init(&argc,&argv);
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Comm_rank(comm,&rank);
  MPI_Comm_size(comm,&nprocs);
  { // Limit scope so variables that contain communicators will free before MPI_Finialize

    if ( rank==0 ) {
        printf("-----------------------------------------------------------\n");
        printf("Unit test for torus to sphere evolution  \n");
        printf("-----------------------------------------------------------\n");
    }

    //.......................................................................
    // Reading the domain information file
    //.......................................................................
    int i,j,k,n;

    // Load inputs
    auto db = loadInputs( nprocs );
    int Nx = db->getVector<int>( "n" )[0];
    int Ny = db->getVector<int>( "n" )[1];
    int Nz = db->getVector<int>( "n" )[2];
    int nprocx = db->getVector<int>( "nproc" )[0];
    int nprocy = db->getVector<int>( "nproc" )[1];
    int nprocz = db->getVector<int>( "nproc" )[2];

    if (rank==0){
    	printf("********************************************************\n");
    	printf("Sub-domain size = %i x %i x %i\n",Nx,Ny,Nz);
    	printf("********************************************************\n");
    }

    // Get the rank info
    auto Dm = std::make_shared<Domain>(db,comm);

    Nx += 2;
    Ny += 2;
    Nz += 2;
    //.......................................................................
    for ( k=1;k<Nz-1;k++){
    	for ( j=1;j<Ny-1;j++){
    		for ( i=1;i<Nx-1;i++){
    			n = k*Nx*Ny+j*Nx+i;
    			Dm->id[n] = 1;
    		}
		}
	}
	//.......................................................................
    Dm->CommInit(); // Initialize communications for domains
	//.......................................................................

    // Create visualization structure
    std::vector<IO::MeshDataStruct> visData;
    fillHalo<double> fillData(Dm->Comm,Dm->rank_info,{Dm->Nx-2,Dm->Ny-2,Dm->Nz-2},{1,1,1},0,1);;    
    
    IO::initialize("","silo","false");
    // Create the MeshDataStruct    
    visData.resize(1);
    visData[0].meshName = "domain";
    visData[0].mesh = std::make_shared<IO::DomainMesh>( Dm->rank_info,Dm->Nx-2,Dm->Ny-2,Dm->Nz-2,Dm->Lx,Dm->Ly,Dm->Lz );
    auto PhaseVar = std::make_shared<IO::Variable>();
    PhaseVar->name = "phase";
    PhaseVar->type = IO::VariableType::VolumeVariable;
    PhaseVar->dim = 1;
    PhaseVar->data.resize(Dm->Nx-2,Dm->Ny-2,Dm->Nz-2);
    visData[0].vars.push_back(PhaseVar);
    
	//.......................................................................
	// Assign the phase ID field based and the signed distance
	//.......................................................................
    double CX,CY,CZ; //CY1,CY2;
    CX=Nx*nprocx*0.5;
    CY=Ny*nprocy*0.5;
    CZ=Nz*nprocz*0.5;
    auto R1 = (Nx-2)*nprocx*0.3; // middle radius
    auto R2 = (Nx-2)*nprocx*0.1; // donut thickness
    //auto R = 0.4*nprocx*(Nx-2);
    
    Minkowski Object(Dm);

    int timestep = 0;
    double time = 0.0;
    double dt = 0.1;
    double x,y,z;
    while (time < 3.0){
    	
    	timestep += 1;
    	time += dt;
    	R1 -= 0.01*nprocx*(Nx-2);
    	R2 += 0.01*nprocx*(Nx-2);
    	
    	if (rank==0) printf("Initializing the system for time = %f, circle radius %f; revolution radius %f \n", time, R2, R1);
    	for ( k=1;k<Nz-1;k++){
    		for ( j=1;j<Ny-1;j++){
    			for ( i=1;i<Nx-1;i++){
    				n = k*Nx*Ny+j*Nx+i;

    				// global position relative to center
    				x = Dm->iproc()*(Nx-2)+i - CX - 0.1;
    				y = Dm->jproc()*(Ny-2)+j - CY - 0.1;
    				z = Dm->kproc()*(Nz-2)+k - CZ -0.1;

    				//..............................................................................
    				// Single torus
    				Object.distance(i,j,k) = sqrt((sqrt(x*x+y*y) - R1)*(sqrt(x*x+y*y) - R1) + z*z) - R2;

    				if (Object.distance(i,j,k) > 0.0){
    					Dm->id[n] = 2;
    					Object.id(i,j,k) = 2;
    				}
    				else{
    					Dm->id[n] = 1;
    					Object.id(i,j,k) = 1;
    				}
    			}
    		}
    	}

        ASSERT(visData[0].vars[0]->name=="phase");
        Array<double>& PhaseData = visData[0].vars[0]->data;
        fillData.copy(Object.distance,PhaseData);
        IO::writeData( timestep, visData, comm );

    	if (rank==0) printf("computing local averages  \n");
    	Object.ComputeScalar(Object.distance,0.0);
    	Object.PrintAll();
    	if (rank==0) printf("reducing averages  \n");

    }
  } // Limit scope so variables that contain communicators will free before MPI_Finialize
  MPI_Barrier(comm);
  MPI_Finalize();
  return 0;  
}

