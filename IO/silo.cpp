#include "IO/silo.h"
#include "common/Utilities.h"
#include "common/MPI_Helpers.h"

#include "ProfilerApp.h"


#ifdef USE_SILO

#include <silo.h>



namespace silo {


/****************************************************
* Helper functions                                  *
****************************************************/
template<class TYPE> static constexpr int getType();
template<> constexpr int getType<double>() { return DB_DOUBLE; }
template<> constexpr int getType<float>() { return DB_FLOAT; }
template<> constexpr int getType<int>() { return DB_INT; }
VariableDataType varDataType( DBfile *fid, const std::string& name )
{
    auto type = DBGetVarType( fid, name.c_str() );
    VariableDataType type2 = VariableDataType::UNKNOWN;
    if ( type == DB_DOUBLE )
        type2 = VariableDataType::DOUBLE;
    else if ( type == DB_FLOAT )
        type2 = VariableDataType::FLOAT;
    else if ( type == DB_INT )
        type2 = VariableDataType::INT;
    return type2;
}
template<class TYPE>
static inline void copyData( Array<TYPE>& data, int type, const void *src )
{
    if ( type == getType<TYPE>() )
        memcpy( data.data(), src, data.length()*sizeof(TYPE) );
    else if ( type == DB_DOUBLE )
        data.copy<double>( static_cast<const double*>(src) );
    else if ( type == DB_FLOAT )
        data.copy<float>( static_cast<const float*>(src) );
    else if ( type == DB_INT )
        data.copy<int>( static_cast<const int*>(src) );
    else
        ERROR("Unknown type");
}


/****************************************************
* Open/close a file                                 *
****************************************************/
DBfile* open( const std::string& filename, FileMode mode )
{
    DBfile *fid = nullptr;
    if ( mode == CREATE ) {
        fid = DBCreate( filename.c_str(), DB_CLOBBER, DB_LOCAL, nullptr, DB_HDF5 );
    } else if ( mode == WRITE ) {
        fid = DBOpen( filename.c_str(), DB_HDF5, DB_APPEND );
    } else if ( mode == READ ) {
        fid = DBOpen( filename.c_str(), DB_HDF5, DB_READ );
    }
    return fid;
}
void close( DBfile* fid )
{
    DBClose( fid );
}


/****************************************************
* Write/read an arbitrary vector                    *
****************************************************/
template<class TYPE> int getSiloType();
template<> int getSiloType<int>() { return DB_INT; }
template<> int getSiloType<float>() { return DB_FLOAT; }
template<> int getSiloType<double>() { return DB_DOUBLE; }
template<class TYPE>
void write( DBfile* fid, const std::string& varname, const std::vector<TYPE>& data )
{
    int dims = data.size();
    int err = DBWrite( fid, varname.c_str(), (void*) data.data(), &dims, 1, getSiloType<TYPE>() );
    ASSERT( err == 0 );
}
template<class TYPE>
std::vector<TYPE> read( DBfile* fid, const std::string& varname )
{
    int N = DBGetVarLength( fid, varname.c_str() );
    std::vector<TYPE> data(N);
    int err = DBReadVar( fid, varname.c_str(), data.data() );
    ASSERT( err == 0 );
    return data;
}


/****************************************************
* Helper function to get variable suffixes          *
****************************************************/
static std::vector<std::string> getVarSuffix( int ndim, int nvars )
{
    std::vector<std::string> suffix(nvars);
    if ( nvars == 1 ) {
        suffix[0] = "";
    } else if ( nvars == ndim ) {
        if ( ndim==2 ) {
            suffix[0] = "_x";
            suffix[1] = "_y";
        } else if ( ndim==3 ) {
            suffix[0] = "_x";
            suffix[1] = "_y";
            suffix[2] = "_z";
        } else {
            ERROR("Not finished");
        }
    } else if ( nvars == ndim*ndim ) {
        if ( ndim==2 ) {
            suffix[0] = "_xx";
            suffix[1] = "_xy";
            suffix[2] = "_yx";
            suffix[3] = "_yy";
        } else if ( ndim==3 ) {
            suffix[0] = "_xx";
            suffix[1] = "_xy";
            suffix[2] = "_xz";
            suffix[3] = "_yx";
            suffix[4] = "_yy";
            suffix[5] = "_yz";
            suffix[6] = "_zx";
            suffix[7] = "_zy";
            suffix[8] = "_zz";
        } else {
            ERROR("Not finished");
        }
    } else {
        for (int i=0; i<nvars; i++)
            suffix[i] = "_" + std::to_string(i+1);
    }
    return suffix;
}


/****************************************************
* Write/read a uniform mesh to silo                 *
****************************************************/
template<int NDIM>
void writeUniformMesh( DBfile* fid, const std::string& meshname,
    const std::array<double,2*NDIM>& range, const std::array<int,NDIM>& N )
{
    PROFILE_START("writeUniformMesh",2);
    int dims[NDIM];
    for (size_t d=0; d<N.size(); d++)
        dims[d] = N[d]+1;
    float *x = nullptr;
    if ( NDIM >= 1 ) {
        x = new float[dims[0]];
        for (int i=0; i<N[0]; i++)
            x[i] = range[0] + i*(range[1]-range[0])/N[0];
        x[N[0]] = range[1];
    }
    float *y = nullptr;
    if ( NDIM >= 2 ) {
        y = new float[dims[1]];
        for (int i=0; i<N[1]; i++)
            y[i] = range[2] + i*(range[3]-range[2])/N[1];
        y[N[1]] = range[3];
    }
    float *z = nullptr;
    if ( NDIM >= 3 ) {
        z = new float[dims[2]];
        for (int i=0; i<N[2]; i++)
            z[i] = range[4] + i*(range[5]-range[4])/N[2];
        z[N[2]] = range[5];
    }
    float *coords[] = { x, y, z };
    int err = DBPutQuadmesh( fid, meshname.c_str(), nullptr, coords, dims, NDIM, DB_FLOAT, DB_COLLINEAR, nullptr );
    ASSERT( err == 0 );
    PROFILE_STOP("writeUniformMesh",2);
}
void readUniformMesh( DBfile* fid, const std::string& meshname,
    std::vector<double>& range, std::vector<int>& N )
{
    DBquadmesh* mesh = DBGetQuadmesh( fid, meshname.c_str() );
    int ndim = mesh->ndims;
    range.resize(2*ndim);
    N.resize(ndim);
    for (int d=0; d<ndim; d++) {
        N[d] = mesh->dims[d]-1;
        range[2*d+0] = mesh->min_extents[d];
        range[2*d+1] = mesh->max_extents[d];
    }
    DBFreeQuadmesh( mesh );
}


/****************************************************
* Write a vector/tensor quad variable               *
****************************************************/
template<int NDIM,class TYPE>
void writeUniformMeshVariable( DBfile* fid, const std::string& meshname, const std::array<int,NDIM>& N,
    const std::string& varname, const Array<TYPE>& data, VariableType type )
{
    PROFILE_START("writeUniformMeshVariable",2);
    int nvars=1, dims[NDIM]={1};
    const TYPE *vars[NDIM] = { nullptr };
    int vartype = 0;
    if ( type == VariableType::NodeVariable ) {
        ASSERT( data.ndim()==NDIM || data.ndim()==NDIM+1 );
        for (int d=0; d<NDIM; d++)
            ASSERT(N[d]+1==(int)data.size(d));
        vartype = DB_NODECENT;
        nvars = data.size(NDIM);
        size_t N = data.length()/nvars;
        for (int d=0; d<NDIM; d++)
            dims[d] = data.size(d);
        for (int i=0; i<nvars; i++)
            vars[i] = &data(i*N);
    } else if ( type == VariableType::EdgeVariable ) {
        ERROR("Not finished");
    } else if ( type == VariableType::SurfaceVariable ) {
        ERROR("Not finished");
    } else if ( type == VariableType::VolumeVariable ) {
        ASSERT( data.ndim()==NDIM || data.ndim()==NDIM+1 );
        for (int d=0; d<NDIM; d++)
            ASSERT(N[d]==(int)data.size(d));
        vartype = DB_ZONECENT;
        nvars = data.size(NDIM);
        size_t N = data.length()/nvars;
        for (int d=0; d<NDIM; d++)
            dims[d] = data.size(d);
        for (int i=0; i<nvars; i++)
            vars[i] = &data(i*N);
    } else {
        ERROR("Invalid variable type");
    }
    auto suffix = getVarSuffix( NDIM, nvars );
    std::vector<std::string> var_names(nvars);
    for (int i=0; i<nvars; i++)
        var_names[i] = varname + suffix[i];
    std::vector<char*> varnames(nvars,nullptr);
    for (int i=0; i<nvars; i++)
        varnames[i] = const_cast<char*>(var_names[i].c_str());
    int err = DBPutQuadvar( fid, varname.c_str(), meshname.c_str(), nvars,
        varnames.data(), vars, dims, NDIM, nullptr, 0, getType<TYPE>(), vartype, nullptr );
    ASSERT( err == 0 );
    PROFILE_STOP("writeUniformMeshVariable",2);
}
template<class TYPE> 
Array<TYPE> readUniformMeshVariable( DBfile* fid, const std::string& varname )
{
    auto var = DBGetQuadvar( fid, varname.c_str() );
    ASSERT( var != nullptr );
    Array<TYPE> data( var->nels, var->nvals );
    int type = var->datatype;
    for (int i=0; i<var->nvals; i++) {
        Array<TYPE> data2( var->nels );
        copyData<TYPE>( data2, type, var->vals[i] );
        memcpy( &data(0,i), data2.data(), var->nels*sizeof(TYPE) );
    }
    DBFreeQuadvar( var );
    std::vector<size_t> dims( var->ndims+1, var->nvals );
    for (int d=0; d<var->ndims; d++)
        dims[d] = var->dims[d];
    data.reshape( dims );
    return data;
}


/****************************************************
* Read/write a point mesh/variable to silo          *
****************************************************/
template<class TYPE>
void writePointMesh( DBfile* fid, const std::string& meshname,
    int ndim, int N, const TYPE *coords[] )
{
    int err = DBPutPointmesh( fid, meshname.c_str(), ndim, coords, N, getType<TYPE>(), nullptr );
    ASSERT( err == 0 );
}
template<class TYPE> 
Array<TYPE> readPointMesh( DBfile* fid, const std::string& meshname )
{
    auto mesh = DBGetPointmesh( fid, meshname.c_str() );
    int N = mesh->nels;
    int ndim = mesh->ndims;
    Array<TYPE> coords(N,ndim);
    int type = mesh->datatype;
    for (int d=0; d<ndim; d++) {
        Array<TYPE> data2( N );
        copyData<TYPE>( data2, type, mesh->coords[d] );
        memcpy( &coords(0,d), data2.data(), N*sizeof(TYPE) );
    }
    DBFreePointmesh( mesh );
    return coords;
}
template<class TYPE>
void writePointMeshVariable( DBfile* fid, const std::string& meshname,
    const std::string& varname, const Array<TYPE>& data )
{
    int N = data.size(0);
    int nvars = data.size(1);
    std::vector<const TYPE*> vars(nvars);
    for (int i=0; i<nvars; i++)
        vars[i] = &data(0,i);
    int err = DBPutPointvar( fid, varname.c_str(), meshname.c_str(), nvars, vars.data(), N, getType<TYPE>(), nullptr );
    ASSERT( err == 0 );
}
template<class TYPE> 
Array<TYPE> readPointMeshVariable( DBfile* fid, const std::string& varname )
{
    auto var = DBGetPointvar( fid, varname.c_str() );
    ASSERT( var != nullptr );
    Array<TYPE> data( var->nels, var->nvals );
    int type = var->datatype;
    for (int i=0; i<var->nvals; i++) {
        Array<TYPE> data2( var->nels );
        copyData<TYPE>( data2, type, var->vals[i] );
        memcpy( &data(0,i), data2.data(), var->nels*sizeof(TYPE) );
    }
    DBFreeMeshvar( var );
    return data;
}


/****************************************************
* Read/write a triangle mesh                        *
****************************************************/
template<class TYPE>
void writeTriMesh( DBfile* fid, const std::string& meshName,
    int ndim, int ndim_tri, int N, const TYPE *coords[], int N_tri, const int *tri[] )
{
    auto zoneName = meshName + "_zones";
    std::vector<int> nodelist( (ndim_tri+1)*N_tri );
    for (int i=0, j=0; i<N_tri; i++) {
        for (int d=0; d<ndim_tri+1; d++, j++)
            nodelist[j] = tri[d][i];
    }
    int shapetype = 0;
    if ( ndim_tri==1 )
        shapetype = DB_ZONETYPE_BEAM;
    else if ( ndim_tri==2 )
        shapetype = DB_ZONETYPE_TRIANGLE;
    else if ( ndim_tri==3 )
        shapetype = DB_ZONETYPE_PYRAMID;
    else
        ERROR("Unknown shapetype");
    int shapesize = ndim_tri+1;
    int shapecnt = N_tri;
    DBPutZonelist2( fid, zoneName.c_str(), N_tri, ndim_tri, nodelist.data(),
        nodelist.size(), 0, 0, 0, &shapetype, &shapesize, &shapecnt, 1, nullptr );
    DBPutUcdmesh( fid, meshName.c_str(), ndim, nullptr, coords, N,
        nodelist.size(), zoneName.c_str(), nullptr, getType<TYPE>(), nullptr );
}
template<class TYPE>
void readTriMesh( DBfile* fid, const std::string& meshname, Array<TYPE>& coords, Array<int>& tri )
{
    auto mesh = DBGetUcdmesh( fid, meshname.c_str() );
    int ndim = mesh->ndims;
    int N_nodes = mesh->nnodes;
    coords.resize(N_nodes,ndim);
    int mesh_type = mesh->datatype;
    for (int d=0; d<ndim; d++) {
        Array<TYPE> data2( N_nodes );
        copyData<TYPE>( data2, mesh_type, mesh->coords[d] );
        memcpy( &coords(0,d), data2.data(), N_nodes*sizeof(TYPE) );
    }
    auto zones = mesh->zones;
    int N_zones = zones->nzones;
    int ndim_zones = zones->ndims;
    ASSERT( zones->nshapes==1 );
    int shape_type = zones->shapetype[0];
    int shapesize = zones->shapesize[0];
    tri.resize(N_zones,shapesize);
    for (int i=0; i<N_zones; i++) {
        for (int j=0; j<shapesize; j++)
            tri(i,j) = zones->nodelist[i*shapesize+j];
    }
    DBFreeUcdmesh( mesh );
}
template<class TYPE>
void writeTriMeshVariable( DBfile* fid, int ndim, const std::string& meshname,
    const std::string& varname, const Array<TYPE>& data, VariableType type )
{
    int nvars = 0;
    int vartype = 0;
    const TYPE *vars[10] = { nullptr };
    if ( type == VariableType::NodeVariable ) {
        vartype = DB_NODECENT;
        nvars = data.size(1);
        for (int i=0; i<nvars; i++)
            vars[i] = &data(0,i);
    } else if ( type == VariableType::EdgeVariable ) {
        ERROR("Not finished");
    } else if ( type == VariableType::SurfaceVariable ) {
        ERROR("Not finished");
    } else if ( type == VariableType::VolumeVariable ) {
        vartype = DB_ZONECENT;
        nvars = data.size(1);
        for (int i=0; i<nvars; i++)
            vars[i] = &data(0,i);
    } else {
        ERROR("Invalid variable type");
    }
    auto suffix = getVarSuffix( ndim, nvars );
    std::vector<std::string> var_names(nvars);
    for (int i=0; i<nvars; i++)
        var_names[i] = varname + suffix[i];
    std::vector<char*> varnames(nvars,nullptr);
    for (int i=0; i<nvars; i++)
        varnames[i] = const_cast<char*>(var_names[i].c_str());
    DBPutUcdvar( fid, varname.c_str(), meshname.c_str(), nvars,
        varnames.data(), vars, data.size(0), nullptr, 0, getType<TYPE>(), vartype, nullptr );
}
template<class TYPE>
Array<TYPE> readTriMeshVariable( DBfile* fid, const std::string& varname )
{
    auto var = DBGetUcdvar( fid, varname.c_str() );
    ASSERT( var != nullptr );
    Array<TYPE> data( var->nels, var->nvals );
    int type = var->datatype;
    for (int i=0; i<var->nvals; i++) {
        Array<TYPE> data2( var->nels );
        copyData<TYPE>( data2, type, var->vals[i] );
        memcpy( &data(0,i), data2.data(), var->nels*sizeof(TYPE) );
    }
    DBFreeUcdvar( var );
    return data;
}


/****************************************************
* Write a multimesh                                 *
****************************************************/
void writeMultiMesh( DBfile* fid, const std::string& meshname,
    const std::vector<std::string>& meshNames,
    const std::vector<int>& meshTypes )
{
    std::vector<char*> meshnames(meshNames.size());
    for ( size_t i = 0; i < meshNames.size(); ++i )
        meshnames[i] = (char *) meshNames[i].c_str();
    std::string tree_name = meshname + "_tree";
    DBoptlist *optList    = DBMakeOptlist( 1 );
    DBAddOption( optList, DBOPT_MRGTREE_NAME, (char *) tree_name.c_str() );
    DBPutMultimesh( fid, meshname.c_str(), meshNames.size(), meshnames.data(), (int*) meshTypes.data(), nullptr );
    DBFreeOptlist( optList );
}


/****************************************************
* Write a multivariable                             *
****************************************************/
void writeMultiVar( DBfile* fid, const std::string& varname,
    const std::vector<std::string>& varNames,
    const std::vector<int>& varTypes )
{
    std::vector<char*> varnames(varNames.size(),nullptr);
    for (size_t j=0; j<varNames.size(); j++)
        varnames[j] = const_cast<char*>(varNames[j].c_str());
    DBPutMultivar( fid, varname.c_str(), varNames.size(), varnames.data(), (int*) varTypes.data(), nullptr );
}


}; // silo namespace


// Explicit instantiations
template std::vector<int>    silo::read<int>(    DBfile* fid, const std::string& varname );
template std::vector<float>  silo::read<float>(  DBfile* fid, const std::string& varname );
template std::vector<double> silo::read<double>( DBfile* fid, const std::string& varname );
template void silo::writeUniformMesh<1>( DBfile*, const std::string&, const std::array<double,2>&, const std::array<int,1>& );
template void silo::writeUniformMesh<2>( DBfile*, const std::string&, const std::array<double,4>&, const std::array<int,2>& );
template void silo::writeUniformMesh<3>( DBfile*, const std::string&, const std::array<double,6>&, const std::array<int,3>& );

namespace {
    template<class TYPE>
    static void siloClassInstantiator() {
        // use **all** functions here so that they get instantiated
        const auto type = silo::VariableType::NullVariable;
        Array<TYPE> tmp;
        Array<int> tri;
        silo::write<TYPE>( nullptr, "", std::vector<TYPE>() );
        silo::read<TYPE>( nullptr, "" );
        silo::writeUniformMeshVariable<1,TYPE>( nullptr, "", std::array<int,1>(), "", Array<TYPE>(), type );
        silo::writeUniformMeshVariable<2,TYPE>( nullptr, "", std::array<int,2>(), "", Array<TYPE>(), type );
        silo::writeUniformMeshVariable<3,TYPE>( nullptr, "", std::array<int,3>(), "", Array<TYPE>(), type );
        silo::writePointMesh( nullptr, "", 0, 0, (const TYPE**) nullptr );
        silo::writePointMeshVariable<TYPE>( nullptr, "", "", Array<TYPE>() );
        silo::writeTriMesh<TYPE>( nullptr, "", 0, 0, 0, (const TYPE**) nullptr, 0, (const int**) nullptr );
        silo::writeTriMeshVariable<TYPE>( nullptr, 0, "", "", Array<TYPE>(), type );
        silo::readUniformMeshVariable<TYPE>( nullptr, "" );
        silo::readPointMesh<TYPE>( nullptr, "" );
        silo::readPointMeshVariable<TYPE>( nullptr, "" );
        silo::readTriMesh<TYPE>( nullptr, "", tmp, tri );
        silo::readTriMeshVariable<TYPE>( nullptr, "" );
    };
    template void siloClassInstantiator<double>();
    template void siloClassInstantiator<float>();
    template void siloClassInstantiator<int>();
}

#else

#endif