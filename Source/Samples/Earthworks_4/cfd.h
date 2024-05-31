#pragma once
#include "Falcor.h"

#include "../../external/cereal-master/include/cereal/cereal.hpp"
#include "../../external/cereal-master/include/cereal/archives/binary.hpp"
#include "../../external/cereal-master/include/cereal/archives/json.hpp"
#include "../../external/cereal-master/include/cereal/archives/xml.hpp"
#include "../../external/cereal-master/include/cereal/types/vector.hpp"
#include <fstream>
#include <xinput.h>



#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Exporter.hpp>


using namespace Falcor;

#define cfdW 1024
#define cfdH 128
#define cfdMinAlt 350           // zurichsee - at least 40 - remove for GPU where we can clamp
#define cfdSize 39.0625f        // 40000 / 1024


// in general 16 or 8 Bit on GPU, way good enough
struct _cfd
{
    // temp
    float height[4096][4096];   // should really be temporary inside load

    //float p[cfdH][cfdW][cfdW];  // h, y, x  center of cell
    //float3 s[cfdH][cfdW][cfdW]; // bottom left corner of cell
    //float3 v[cfdH][cfdW][cfdW]; // GPU split into 3 of we re using edges
    //float3 v2[cfdH][cfdW][cfdW]; // GPU split into 3 of we re using edges
    std::vector<float3> s;
    std::vector<float3> v;
    std::vector<float3> v2;

    uint inline idx(int x, int y, int z);
    uint inline idx(uint3 p);

    void init();
    void loadHeight(std::string filename);
    void export_S(std::string filename);
    void import_S(std::string filename);
    void export_V(std::string filename);
    void import_V(std::string filename);

    void setWind(float3 _windBottom = float3(-1, 0, 0), float3 _windTop = float3(-5, 0, 0));

    // simulate
    void gavity(float _dt);
    void solveIncompressibility(uint _num);
    void edges();
    void advect(float _dt);
    float sampleVx(float3 _p);
    float sampleVy(float3 _p);
    float sampleVz(float3 _p);

    void solveIncompressibilityCenter(uint _num);
    void advectCenter(float _dt);
    float3 sample(float3 _p);
    void simulate(float _dt);
    uint tickCount = 0;

    void exportStreamlines(std::string filename, uint3 _p);
};


struct _cfd_map
{
    uint offset;
    glm::u16 bottom;
    glm::u16 size;
};

// ??? can we interpolate an Smap for higer lods
struct _cfd_Smap
{
    void init(uint _w);
    void save(std::string filename);
    void load(std::string filename);
    void mipDown(_cfd_Smap& top);

    uint width;
    const glm::u8vec4 solid = {0, 0, 0, 0};
    const glm::u8vec4 freeair = { 255, 255, 255, 255 };
    std::vector<_cfd_map> map;
    std::vector<glm::u8vec4> S;     //sx, sy, sz, volume

    glm::u8vec4 getS(uint3 _p);
};


struct _cfd_lod
{
    void init(uint _w, uint _h, float _worldSize);
    inline uint idx(uint3 _p) { return _p.y * w2 + _p.z * width + _p.x;  }  // do a safe version
    inline uint idx(uint x, uint y, uint z) { return y * w2 + z * width + x; }  // do a safe version
    float3& getV(uint3 _p) { return v[idx(_p)]; }
    void setV(uint3 _p, float3 _v);

    // lodding
    void fromRoot();
    void fromRootEdges();
    void toRoot();

    // simulate
    void incompressibility(uint _num);
    void edges();
    void advect(float _dt);
    float3 sample(float3 _p);
    void clamp(float3 &_p);
    float sample_x(float3 _p);
    float sample_y(float3 _p);
    float sample_z(float3 _p);
    float3 sample_xyz(float3 _p);
    void simulate(float _dt);
    void streamlines(float3 _p, float4* _data);

    uint3 offset = {0, 0, 0};
    _cfd_lod* root = nullptr;

    uint width, w2;
    uint height;
    float cellSize;
    float oneOverSize;
    uint tickCount = 0;
    float frameTime = 0;

    _cfd_Smap smap;
    std::vector<float3> v;
    std::vector<float3> v2;
};


struct _cfdClipmap
{
    

    void heightToSmap(std::string filename);
    void build(std::string _path);
    void setWind(float3 _bottom = float3(-1, 0, 0), float3 _top = float3(-5, 0, 0));

    void simulate_start(float _dt);
    void simulate(float _dt);
    uint counter = 0;
    void streamlines(float3 _p, float4* _data);


    std::array<_cfd_lod, 6>    lods;
    /*
    uint inline idx(int x, int y, int z);
    uint inline idx(uint3 p);
    void init();
    void loadHeight(std::string filename);
    void export_S(std::string filename);
    void import_S(std::string filename);
    void export_V(std::string filename);
    void import_V(std::string filename);

    void setWind(float3 _windBottom = float3(-1, 0, 0), float3 _windTop = float3(-5, 0, 0));

    // simulate
    void gavity(float _dt);
    void incompressibility(uint _num);
    void edges();
    void advect(float _dt);
    float3 sample(float3 _p);
    void simulate(float _dt);

    void buildStreamlines(std::string filename, uint3 _p);
    */
};
