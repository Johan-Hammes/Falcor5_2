#pragma once
#include "Falcor.h"

#include "../../external/cereal-master/include/cereal/cereal.hpp"
#include "../../external/cereal-master/include/cereal/archives/binary.hpp"
#include "../../external/cereal-master/include/cereal/archives/json.hpp"
#include "../../external/cereal-master/include/cereal/archives/xml.hpp"
#include "../../external/cereal-master/include/cereal/types/vector.hpp"
#include <fstream>
#include <xinput.h>
#include <chrono>
#include <random>



#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Exporter.hpp>


using namespace Falcor;


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
    void saveNormals(std::string filename);
    

    uint width;
    const glm::u8vec4 solid = {0, 0, 0, 0};
    const glm::u8vec4 freeair = { 255, 255, 255, 255 };
    std::vector<_cfd_map> map;
    std::vector<glm::u8vec4> S;     //sx, sy, sz, volume

    glm::u8vec4 getS(uint3 _p);

    std::vector<float4> normals;
};


struct _cfd_lod
{
    void init(uint _w, uint _h, float _worldSize);
    inline uint idx(uint3 _p) { return _p.y * w2 + _p.z * width + _p.x;  }  // do a safe version
    inline uint idx(uint x, uint y, uint z) { return y * w2 + z * width + x; }  // do a safe version
    float3& getV(uint3 _p) { return v[idx(_p)]; }
    void setV(uint3 _p, float3 _v);

    void loadNormals(std::string filename);
    std::vector<float4> normals;

    float3 to_Root;
    float3 from_Root;

    // lodding
    void fromRoot();


    void fromRoot_Alpha();
    void toRoot_Alpha();

    void export_V(std::string filename);
    bool import_V(std::string filename);

    // simulate
    void incompressibility_SMAP(uint _num);
    void Normal();
    void incompressibilityNormal(uint _num);
    void edges();
    void advect(float _dt);
    void vorticty_confine(float _dt);
    float3 sample(float3 _p);
    float3 sampleCurl(float3 _p);
    void clamp(float3 &_p);
    //float sample_x(float3 _p);
    //float sample_y(float3 _p);
    //float sample_z(float3 _p);
    //float3 sample_xyz(float3 _p);
    void simulate(float _dt);

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

    std::vector<float3> curl;
    std::vector<float> mag;
    std::vector<float3> vorticity;

    double simTimeLod_advect_ms;
    double simTimeLod_incompress_ms;
    double simTimeLod_vorticity_ms;
    double simTimeLod_edges_ms;
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

    void export_V(std::string _name);
    bool import_V(std::string _name);


    std::array<_cfd_lod, 6>    lods;

    double simTimeLod_ms;
    
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