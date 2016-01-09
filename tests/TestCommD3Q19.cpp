//*************************************************************************
// Lattice Boltzmann Simulator for Single Phase Flow in Porous Media
// James E. McCLure
//*************************************************************************
#include <stdio.h>
#include <iostream>
#include <fstream>
#include "common/ScaLBL.h"
#include "common/MPI_Helpers.h"

using namespace std;


extern void GlobalFlipInitD3Q19(double *dist_even, double *dist_odd, int Nx, int Ny, int Nz, 
								int iproc, int jproc, int kproc, int nprocx, int nprocy, int nprocz)
{
	// Set of Discrete velocities for the D3Q19 Model
	static int D3Q19[18][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
	{1,1,0},{-1,-1,0},{1,-1,0},{-1,1,0},{1,0,1},{-1,0,-1},{1,0,-1},{-1,0,1},
	{0,1,1},{0,-1,-1},{0,1,-1},{0,-1,1}};
	
	int q,i,j,k,n,N;
	int Cqx,Cqy,Cqz; // Discrete velocity
	int x,y,z;		// Global indices
	int xn,yn,zn; 	// Global indices of neighbor 
	int X,Y,Z;		// Global size
	X = Nx*nprocx;
	Y = Ny*nprocy;
	Z = Nz*nprocz;
    NULL_USE(Z);
	N = (Nx+2)*(Ny+2)*(Nz+2);	// size of the array including halo
	for (k=0; k<Nz; k++){ 
		for (j=0; j<Ny; j++){
			for (i=0; i<Nx; i++){
				
				n = (k+1)*(Nx+2)*(Ny+2) + (j+1)*(Nx+2) + i+1;
				
				// Get the 'global' index
				x = iproc*Nx+i;
				y = jproc*Ny+j;
				z = kproc*Nz+k;
				for (q=0; q<9; q++){
					// Odd distribution
					Cqx = D3Q19[2*q][0];
					Cqy = D3Q19[2*q][1];
					Cqz = D3Q19[2*q][2];
					xn = x - Cqx;
					yn = y - Cqy;
					zn = z - Cqz;
					if (xn < 0) xn += nprocx*Nx;
					if (yn < 0) yn += nprocy*Ny;
					if (zn < 0) zn += nprocz*Nz;
					if (!(xn < nprocx*Nx)) xn -= nprocx*Nx;
					if (!(yn < nprocy*Ny)) yn -= nprocy*Ny;
					if (!(zn < nprocz*Nz)) zn -= nprocz*Nz;	
					
					dist_even[(q+1)*N+n] = (zn*X*Y+yn*X+xn) + (2*q+1)*0.01;
					
					// Odd distribution
					xn = x + Cqx;
					yn = y + Cqy;
					zn = z + Cqz;
					if (xn < 0) xn += nprocx*Nx;
					if (yn < 0) yn += nprocy*Ny;
					if (zn < 0) zn += nprocz*Nz;
					if (!(xn < nprocx*Nx)) xn -= nprocx*Nx;
					if (!(yn < nprocy*Ny)) yn -= nprocy*Ny;
					if (!(zn < nprocz*Nz)) zn -= nprocz*Nz;
					
					dist_odd[q*N+n] =  (zn*X*Y+yn*X+xn) + 2*(q+1)*0.01;
				
				}
			}
		}
	}
	
}

extern int GlobalCheckDebugDist(double *dist_even, double *dist_odd, int Nx, int Ny, int Nz, 
		int iproc, int jproc, int kproc, int nprocx, int nprocy, int nprocz)
{

	int returnValue = 0;
	int q,i,j,k,n,N;
	int Cqx,Cqy,Cqz; // Discrete velocity
	int x,y,z;		// Global indices
	int xn,yn,zn; 	// Global indices of neighbor 
	int X,Y,Z;		// Global size
	X = Nx*nprocx;
	Y = Ny*nprocy;
	Z = Nz*nprocz;
    NULL_USE(Z);
	N = (Nx+2)*(Ny+2)*(Nz+2);	// size of the array including halo
	for (k=0; k<Nz; k++){ 
		for (j=0; j<Ny; j++){
			for (i=0; i<Nx; i++){

				n = (k+1)*(Nx+2)*(Ny+2) + (j+1)*(Nx+2) + i+1;

				// Get the 'global' index
				x = iproc*Nx+i;
				y = jproc*Ny+j;
				z = kproc*Nz+k;
				for (q=0; q<9; q++){

					if (dist_even[(q+1)*N+n] != (z*X*Y+y*X+x) + 2*(q+1)*0.01){
						printf("******************************************\n");
						printf("error in even distribution q = %i \n", 2*(q+1));
						printf("i,j,k= %i, %i, %i \n", x,y,z);
						printf("dist = %5.2f \n", dist_even[(q+1)*N+n]);
						printf("n= %i \n",z*X*Y+y*X+x);
						returnValue++;
					}


					if (dist_odd[q*N+n] !=  (z*X*Y+y*X+x) + (2*q+1)*0.01){
						printf("******************************************\n");
						printf("error in odd distribution q = %i \n", 2*q+1);
						printf("i,j,k= %i, %i, %i \n", x,y,z);
						printf("dist = %5.2f \n", dist_odd[q*N+n]);
						printf("n= %i \n",z*X*Y+y*X+x);
						returnValue++;
					}
				}
			}
		}
	}
	return returnValue;
}

inline void PackID(int *list, int count, char *sendbuf, char *ID){
	// Fill in the phase ID values from neighboring processors
	// This packs up the values that need to be sent from one processor to another
	int idx,n;

	for (idx=0; idx<count; idx++){
		n = list[idx];
		sendbuf[idx] = ID[n];
	}
}
//***************************************************************************************
inline void UnpackID(int *list, int count, char *recvbuf, char *ID){
	// Fill in the phase ID values from neighboring processors
	// This unpacks the values once they have been recieved from neighbors
	int idx,n;

	for (idx=0; idx<count; idx++){
		n = list[idx];
		ID[n] = recvbuf[idx];
	}
}
//***************************************************************************************
int main(int argc, char **argv)
{
	//*****************************************
	// ***** MPI STUFF ****************
	//*****************************************
	// Initialize MPI
	int rank,nprocs;
	MPI_Init(&argc,&argv);
	MPI_Comm comm = MPI_COMM_WORLD;
	MPI_Comm_rank(comm,&rank);
	MPI_Comm_size(comm,&nprocs);
	int check;

	{
		// parallel domain size (# of sub-domains)
		int nprocx,nprocy,nprocz;
		int iproc,jproc,kproc;
		//*****************************************
		// MPI ranks for all 18 neighbors
		//**********************************
		int rank_x,rank_y,rank_z,rank_X,rank_Y,rank_Z;
		int rank_xy,rank_XY,rank_xY,rank_Xy;
		int rank_xz,rank_XZ,rank_xZ,rank_Xz;
		int rank_yz,rank_YZ,rank_yZ,rank_Yz;
		//**********************************
		MPI_Request req1[18],req2[18];
		MPI_Status stat1[18],stat2[18];

		if (rank == 0){
			printf("********************************************************\n");
			printf("Running Unit Test for D3Q19 MPI Communication	\n");
			printf("********************************************************\n");
		}

		// BGK Model parameters
		string FILENAME;
		unsigned int nBlocks, nthreads;
		int timestepMax, interval;
		double tau,Fx,Fy,Fz,tol;
		// Domain variables
		double Lx,Ly,Lz;
		int nspheres;
		int Nx,Ny,Nz;
		int i,j,k,n;

		if (rank==0){
			//.......................................................................
			// Reading the domain information file
			//.......................................................................
			ifstream domain("Domain.in");
			if (domain.good()){
				domain >> nprocx;
				domain >> nprocy;
				domain >> nprocz;
				domain >> Nx;
				domain >> Ny;
				domain >> Nz;
				domain >> nspheres;
				domain >> Lx;
				domain >> Ly;
				domain >> Lz;
			}
			else if (nprocs==1){
				nprocx=nprocy=nprocz=1;
				Nx=Ny=Nz=50;
				nspheres=0;
				Lx=Ly=Lz=1;
			}
			else if (nprocs==2){
				nprocx=nprocy=1;
				nprocz=2;
				Nx=Ny=Nz=50;
				nspheres=0;
				Lx=Ly=Lz=1;
			}
			else if (nprocs==4){
				nprocx=nprocy=2;
				nprocz=1;
				Nx=Ny=Nz=50;
				nspheres=0;
				Lx=Ly=Lz=1;
			}
			else if (nprocs==8){
				nprocx=nprocy=nprocz=2;
				Nx=Ny=Nz=50;
				nspheres=0;
				Lx=Ly=Lz=1;
			}
			//.......................................................................
		}
		// **************************************************************
		// Broadcast simulation parameters from rank 0 to all other procs
		MPI_Barrier(comm);
		//.................................................
		MPI_Bcast(&Nx,1,MPI_INT,0,comm);
		MPI_Bcast(&Ny,1,MPI_INT,0,comm);
		MPI_Bcast(&Nz,1,MPI_INT,0,comm);
		MPI_Bcast(&nBlocks,1,MPI_INT,0,comm);
		MPI_Bcast(&nthreads,1,MPI_INT,0,comm);
		MPI_Bcast(&timestepMax,1,MPI_INT,0,comm);

		MPI_Bcast(&Nx,1,MPI_INT,0,comm);
		MPI_Bcast(&Ny,1,MPI_INT,0,comm);
		MPI_Bcast(&Nz,1,MPI_INT,0,comm);
		MPI_Bcast(&nprocx,1,MPI_INT,0,comm);
		MPI_Bcast(&nprocy,1,MPI_INT,0,comm);
		MPI_Bcast(&nprocz,1,MPI_INT,0,comm);
		MPI_Bcast(&nspheres,1,MPI_INT,0,comm);
		MPI_Bcast(&Lx,1,MPI_DOUBLE,0,comm);
		MPI_Bcast(&Ly,1,MPI_DOUBLE,0,comm);
		MPI_Bcast(&Lz,1,MPI_DOUBLE,0,comm);
		//.................................................
		MPI_Barrier(comm);
		// **************************************************************
		// **************************************************************

		if (nprocs != nprocx*nprocy*nprocz){
			printf("nprocx =  %i \n",nprocx);
			printf("nprocy =  %i \n",nprocy);
			printf("nprocz =  %i \n",nprocz);
			INSIST(nprocs == nprocx*nprocy*nprocz,"Fatal error in processor count!");
		}

		if (rank==0){
			printf("********************************************************\n");
			printf("Sub-domain size = %i x %i x %i\n",Nz,Nz,Nz);
			printf("Parallel domain size = %i x %i x %i\n",nprocx,nprocy,nprocz);
			printf("********************************************************\n");
		}

		MPI_Barrier(comm);
		kproc = rank/(nprocx*nprocy);
		jproc = (rank-nprocx*nprocy*kproc)/nprocx;
		iproc = rank-nprocx*nprocy*kproc-nprocz*jproc;

		double iVol_global = 1.0/Nx/Ny/Nz/nprocx/nprocy/nprocz;
		int BoundaryCondition=0;
		Domain Dm(Nx,Ny,Nz,rank,nprocx,nprocy,nprocz,Lx,Ly,Lz,BoundaryCondition);

		InitializeRanks( rank, nprocx, nprocy, nprocz, iproc, jproc, kproc,
				rank_x, rank_y, rank_z, rank_X, rank_Y, rank_Z,
				rank_xy, rank_XY, rank_xY, rank_Xy, rank_xz, rank_XZ, rank_xZ, rank_Xz,
				rank_yz, rank_YZ, rank_yZ, rank_Yz );

		Nx += 2;
		Ny += 2;
		Nz += 2;
		int N = Nx*Ny*Nz;
		int dist_mem_size = N*sizeof(double);

		//.......................................................................
		// Assign the phase ID field
		//.......................................................................
		char LocalRankString[8];
		sprintf(LocalRankString,"%05d",rank);
		char LocalRankFilename[40];
		sprintf(LocalRankFilename,"ID.%05i",rank);

		char *id;
		id = new char[Nx*Ny*Nz];

		/*
		 * 	if (rank==0) printf("Assigning phase ID from file \n");
		 * 	if (rank==0) printf("Initialize from segmented data: solid=0, NWP=1, WP=2 \n");
	FILE *IDFILE = fopen(LocalRankFilename,"rb");
	if (IDFILE==NULL) ERROR("Error opening file: ID.xxxxx");
	fread(id,1,N,IDFILE);
	fclose(IDFILE);
		 */
		// Setup the domain
		for (k=0;k<Nz;k++){
			for (j=0;j<Ny;j++){
				for (i=0;i<Nx;i++){
					n = k*Nx*Ny+j*Nx+i;
					id[n] = 1;
					Dm.id[n] = id[n];
				}
			}
		}
		Dm.CommInit(comm);

		//.......................................................................
		// Compute the media porosity
		//.......................................................................
		double sum;
		double sum_local=0.0, porosity;
		char component = 0; // solid phase
		for (k=1;k<Nz-1;k++){
			for (j=1;j<Ny-1;j++){
				for (i=1;i<Nx-1;i++){
					n = k*Nx*Ny+j*Nx+i;
					if (id[n] == component){
						sum_local+=1.0;
					}
				}
			}
		}
		MPI_Allreduce(&sum_local,&sum,1,MPI_DOUBLE,MPI_SUM,comm);
		porosity = 1.0-sum*iVol_global;
		if (rank==0) printf("Media porosity = %f \n",porosity);
		//.......................................................................

		//...........................................................................
		MPI_Barrier(comm);
		if (rank == 0) cout << "Domain set." << endl;
		//...........................................................................

		//...........................................................................
		if (rank==0)	printf ("Create ScaLBL_Communicator \n");
		// Create a communicator for the device
		ScaLBL_Communicator ScaLBL_Comm(Dm);

		//...........device phase ID.................................................
		if (rank==0)	printf ("Copying phase ID to device \n");
		char *ID;
		AllocateDeviceMemory((void **) &ID, N);						// Allocate device memory
		// Copy to the device
		CopyToDevice(ID, id, N);
		//...........................................................................

		//...........................................................................
		//				MAIN  VARIABLES ALLOCATED HERE
		//...........................................................................
		// LBM variables
		if (rank==0)	printf ("Allocating distributions \n");
		//......................device distributions.................................
		double *f_even,*f_odd;
		//...........................................................................
		AllocateDeviceMemory((void **) &f_even, 10*dist_mem_size);	// Allocate device memory
		AllocateDeviceMemory((void **) &f_odd, 9*dist_mem_size);	// Allocate device memory
		//...........................................................................
		double *f_even_host,*f_odd_host;
		f_even_host = new double [10*N];
		f_odd_host = new double [9*N];
		//...........................................................................

		/*	// Write the communcation structure into a file for debugging
	char LocalCommFile[40];
	sprintf(LocalCommFile,"%s%s","Comm.",LocalRankString);
	FILE *CommFile;
	CommFile = fopen(LocalCommFile,"w");
	fprintf(CommFile,"rank=%d, ",rank);
	fprintf(CommFile,"i=%d,j=%d,k=%d :",iproc,jproc,kproc);
	fprintf(CommFile,"x=%d, ",rank_x);
	fprintf(CommFile,"X=%d, ",rank_X);
	fprintf(CommFile,"y=%d, ",rank_y);
	fprintf(CommFile,"Y=%d, ",rank_Y);
	fprintf(CommFile,"z=%d, ",rank_z);
	fprintf(CommFile,"Z=%d, ",rank_Z);
	fprintf(CommFile,"xy=%d, ",rank_xy);
	fprintf(CommFile,"XY=%d, ",rank_XY);
	fprintf(CommFile,"xY=%d, ",rank_xY);
	fprintf(CommFile,"Xy=%d, ",rank_Xy);
	fprintf(CommFile,"xz=%d, ",rank_xz);
	fprintf(CommFile,"XZ=%d, ",rank_XZ);
	fprintf(CommFile,"xZ=%d, ",rank_xZ);
	fprintf(CommFile,"Xz=%d, ",rank_Xz);
	fprintf(CommFile,"yz=%d, ",rank_yz);
	fprintf(CommFile,"YZ=%d, ",rank_YZ);
	fprintf(CommFile,"yZ=%d, ",rank_yZ);
	fprintf(CommFile,"Yz=%d, ",rank_Yz);
	fprintf(CommFile,"\n");
	fclose(CommFile);
		 */
		if (rank==0)	printf("Setting the distributions, size = : %i\n", N);
		//...........................................................................
		GlobalFlipInitD3Q19(f_even_host, f_odd_host, Nx-2, Ny-2, Nz-2,iproc,jproc,kproc,nprocx,nprocy,nprocz);
		CopyToDevice(f_even, f_even_host, 10*dist_mem_size);
		CopyToDevice(f_odd, f_odd_host, 9*dist_mem_size);
		DeviceBarrier();
		MPI_Barrier(comm);
		//*************************************************************************
		// Pack and send the D3Q19 distributions
		ScaLBL_Comm.SendD3Q19(f_even, f_odd);
		//*************************************************************************
		// 		Swap the distributions for momentum transport
		//*************************************************************************
		SwapD3Q19(ID, f_even, f_odd, Nx, Ny, Nz);
		//*************************************************************************
		// Wait for communications to complete and unpack the distributions
		ScaLBL_Comm.RecvD3Q19(f_even, f_odd);
		//*************************************************************************

		//...........................................................................
		CopyToHost(f_even_host,f_even,10*N*sizeof(double));
		CopyToHost(f_odd_host,f_odd,9*N*sizeof(double));
		check =	GlobalCheckDebugDist(f_even_host, f_odd_host, Nx-2, Ny-2, Nz-2,iproc,jproc,kproc,nprocx,nprocy,nprocz);
		//...........................................................................

		int timestep = 0;
		if (rank==0) printf("********************************************************\n");
		if (rank==0)	printf("No. of timesteps for timing: %i \n", 100);

		//.......create and start timer............
		double starttime,stoptime,cputime;
		MPI_Barrier(comm);
		starttime = MPI_Wtime();
		//.........................................


		//************ MAIN ITERATION LOOP (timing communications)***************************************/
		while (timestep < 100){

			//*************************************************************************
			// Pack and send the D3Q19 distributions
			ScaLBL_Comm.SendD3Q19(f_even, f_odd);
			//*************************************************************************
			// 		Swap the distributions for momentum transport
			//*************************************************************************
			SwapD3Q19(ID, f_even, f_odd, Nx, Ny, Nz);
			//*************************************************************************
			// Wait for communications to complete and unpack the distributions
			ScaLBL_Comm.RecvD3Q19(f_even, f_odd);
			//*************************************************************************

			DeviceBarrier();
			MPI_Barrier(comm);
			// Iteration completed!
			timestep++;
			//...................................................................
		}
		//************************************************************************/
		stoptime = MPI_Wtime();
		//	cout << "CPU time: " << (stoptime - starttime) << " seconds" << endl;
		cputime = stoptime - starttime;
		//	cout << "Lattice update rate: "<< double(Nx*Ny*Nz*timestep)/cputime/1000000 <<  " MLUPS" << endl;
		double MLUPS = double(Nx*Ny*Nz*timestep)/cputime/1000000;
		if (rank==0) printf("********************************************************\n");
		if (rank==0) printf("CPU time = %f \n", cputime);
		if (rank==0) printf("Lattice update rate (per process)= %f MLUPS \n", MLUPS);
		MLUPS *= nprocs;
		if (rank==0) printf("Lattice update rate (process)= %f MLUPS \n", MLUPS);
		if (rank==0) printf("********************************************************\n");

		// Number of memory references from the swap algorithm (per timestep)
		// 18 reads and 18 writes for each lattice site
		double MemoryRefs = (Nx-2)*(Ny-2)*(Nz-2)*36;
		// number of memory references for the swap algorithm - GigaBytes / second
		if (rank==0) printf("DRAM bandwidth (per process)= %f GB/sec \n",MemoryRefs*8*timestep/1e9);
		// Report bandwidth in Gigabits per second
		// communication bandwidth includes both send and recieve
		if (rank==0) printf("Communication bandwidth (per process)= %f Gbit/sec \n",ScaLBL_Comm.CommunicationCount*64*timestep/1e9);
		if (rank==0) printf("Aggregated communication bandwidth = %f Gbit/sec \n",nprocs*ScaLBL_Comm.CommunicationCount*64*timestep/1e9);
	}
	// ****************************************************
	MPI_Barrier(comm);
	MPI_Finalize();
	// ****************************************************

	return check;
}
