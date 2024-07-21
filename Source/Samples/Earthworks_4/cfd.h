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


// so for some odd reason float3 seems fastest, lets stick to it then, almost twice as fast as glm::i16vec3
using cfd_V_type = float3;// float3;        
using cfd_P_type = float;// float if V is lfoat;
using cfd_data_type = float3;     //temperature, humidity, waterdroplets, I think


inline float to_K(float c) { return c + 273.15f; }
inline float to_C(float k) { return k - 273.15f; }

// in Celcius
inline float dew_Y(float RH32)
{
    //return log(RH32) + 17.625 * (32.f) / (243.04 + 32.f);
    return log(RH32) + 2.1942110177f;
}

inline float dew_Temp_C(float RH32)
{
    float y = dew_Y(RH32);
    return (243.04f * y) / (17.625f - y);
}


#define cfd_g  9.8076f
#define Hv 2501000.f
#define Rsd 287.f
#define Rsw 461.5f
#define Eps 0.622f
#define Cpd 1003.5f

///https://www.engineeringtoolbox.com/air-altitude-pressure-d_462.html
inline float pascals_air(float h)
{
    return 101325 * pow(1.f - (0.0000225577f * h), 5.25588);
}

///https://en.wikipedia.org/wiki/Vapour_pressure_of_water
inline float mmHg_Antoine(float T_c)
{
    return pow(10, 8.07131f - (1730.63 / (233.426 + T_c)));
}

inline float pascals_Antoine(float T_c)
{
    return 133.322f * mmHg_Antoine(T_c);
}

///https://en.wikipedia.org/wiki/Lapse_rate
inline float moistLapse(float T, float alt)
{
    float e = pascals_Antoine(to_C(T));
    float r = (Eps * e) / (pascals_air(alt) - e);
    return cfd_g * (1.f + (Hv * r) / (Rsd * T)) / (Cpd + (Hv * Hv * r) / (Rsw * T * T));
}



// https://www.engineeringtoolbox.com/air-altitude-pressure-d_462.html
// or    https://en.wikipedia.org/wiki/Atmospheric_pressure
inline float pressure_Pa(float _alt)
{
    //p = 101325 (1 - 2.25577 10-5 h)5.25588
    //return 101325.f * pow(1.f - 0.000025577f * _alt, 5.25588);

    //return 101325 * exp( -9.80665f * _alt * 0.02896968f / (288.16f * 8.314462618f));
    return 101325 * exp(-9.80665f * _alt * 0.02896968f / (303.16f * 8.314462618f));
}

//https://en.wikipedia.org/wiki/Density_of_air
inline float density(float _alt, float _T_K)
{
    return 0.00348371281f * pressure_Pa(_alt) / _T_K;
    //return (pressure_Pa(_alt) * 0.0289652f) / (8.31446261815324f * _T_k);
}


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

    void setV(uint3 _p, cfd_V_type _v, cfd_data_type _data);
    void shiftOrigin(int3 _shift);
    float3 getV(uint _idx) { return  v[_idx];}

    // lodding
    void fromRoot();
    void fromRoot_Alpha();
    void toRoot_Alpha();

    // simulate
    void clamp(float3& _p);
    template <typename T> T sample(std::vector<T>& data, float3 _p);
    void addTemperature();
    void bouyancy(float _dt);
    void incompressibility_SMAP();
    void advect(float _dt);
    void diffuse(float _dt);
    void edges();
    void solveBlockRating();
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
    std::vector<cfd_data_type> data;
    std::vector<unsigned char> blockRating;           // uint32 bitmapped bool
    std::vector<uint3> blocks;

    static std::vector<cfd_V_type> root_v;



    uint numBlocks = 0;
    double simTimeLod_blocks_ms;
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
    void loadNormals(std::string filename);
    std::vector<float4> normals;
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

    bool showStreamlines = false;
    float stremlineScale = 0.05f;
    float streamlineAreaScale = 1.f;
    float streamLinesSize = 100;
    float streamLinesOffset = 1000;

    // sliceViz
    bool showSlice = false;
    bool showskewT = false;
    int slicelod = 5;
    int sliceIndex = 64;
    std::array<uint, 128 * 128> arrayVisualize;
    std::array<float3, 128 * 128> sliceV;
    std::array<float3, 128 * 128> sliceData;
    bool sliceNew = false;
    float3 sliceCorners[4];
    std::array<float3, 100> skewTData;
    std::array<float3, 100> skewTV;

    
    


    std::array<_cfd_lod, 6>    lods;

    double simTimeLod_ms;


    static float incompres_relax;
    static int incompres_loop;
    static float vort_confine;

    static std::vector<cfd_V_type> v_back;      // my scratch buffers
    static std::vector<cfd_V_type> v_bfecc;

    static std::vector<cfd_data_type> data_back;      // my scratch buffers
    static std::vector<cfd_data_type> data_bfecc;

    static std::vector<cfd_V_type> curl;
    static std::vector<float> mag;
    static std::vector<cfd_V_type> vorticity;
    
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
