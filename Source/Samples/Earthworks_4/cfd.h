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


using cfd_V_type = float4;
using cfd_data_type = glm::i16vec3;     //temperature, humidity, waterdroplets, I think
#define cfd_VelocityScale 1.f;

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
    void init(uint _w, uint _h, float _worldSize, float scale);
    void export_V(std::string filename);
    bool import_V(std::string filename);

    inline uint idx(uint x, uint y, uint z) { return ((y + offset.y) % height) * w2 + ((z + offset.z) % width) * width + ((x + offset.x) % width); }  // do a safe version
    inline uint h_idx(uint y) { return ((y + offset.y) % height) * w2; }
    inline uint z_idx(uint z) { return ((z + offset.z) % width) * width; }

    void setV(uint3 _p, float3 _v);
    void shiftOrigin(int3 _shift);

    // lodding
    void fromRoot();
    void fromRoot_Alpha();
    void toRoot_Alpha();

    // simulate
    void clamp(float3& _p);
    template <typename T> T sampleNew(std::vector<T>& data, float3 _p);
    void bouyancy(float _dt);
    void incompressibility_SMAP();
    void advect(float _dt);
    void edges();
    void simulate(float _dt);
    

    int3 offset = {0, 0, 0};
    _cfd_lod* root = nullptr;

    uint width;
    uint w2;        // width ^ 2
    uint height;
    float cellSize;
    float oneOverSize;
    uint tickCount = 0;
    float frameTime = 0;

    _cfd_Smap smap;
    std::vector<cfd_V_type> v;



    double simTimeLod_advect_ms;
    double simTimeLod_incompress_ms;
    double simTimeLod_boyancy_ms;
    double simTimeLod_edges_ms;

    float timer = 0.f;
    float maxSpeed;
    float maxStep;
    float solveTime_ms;
    float maxP;

    // deprecated
    //void Normal();
    //void incompressibility_Normal(uint _num);
    //void vorticty_confine(float _dt, float _scale);
    //    std::vector<float3> curl;   // moce to paretn class 
    //    std::vector<float> mag;
    //    std::vector<float3> vorticity;
    //void loadNormals(std::string filename);
    //std::vector<float4> normals;
};


struct _cfdClipmap
{
    void requestNewWind(float3 wind) { windrequest = true; newWind = wind; }
    bool windrequest = false;
    float3 newWind = {5.f, 0, 0};

    void heightToSmap(std::string filename);
    void build(std::string _path);
    void setWind(float3 _bottom = float3(-1, 0, 0), float3 _top = float3(-5, 0, 0));

    void simulate_start(float _dt);
    void simulate(float _dt);
    uint counter = 0;
    void streamlines(float3 _p, float4* _data, float3 right);

    void export_V(std::string _name);
    bool import_V(std::string _name);

    void shiftOrigin(float3 _origin);

    bool showStreamlines = true;
    float stremlineScale = 0.05f;
    float streamlineAreaScale = 1.f;
    float streamLinesSize = 100;
    float streamLinesOffset = 1000;

    // sliceViz
    bool showSlice = true;
    int slicelod = 0;
    int sliceIndex = 64;
    std::array<uint, 128 * 128> arrayVisualize;

    
    


    std::array<_cfd_lod, 6>    lods;

    double simTimeLod_ms;


    static float incompres_relax;
    static int incompres_loop;
    static std::vector<cfd_V_type> v_back;      // my scratch buffers
    static std::vector<cfd_V_type> v_bfecc;

    static std::vector<cfd_data_type> data_back;      // my scratch buffers
    static std::vector<cfd_data_type> data_bfecc;
    
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
