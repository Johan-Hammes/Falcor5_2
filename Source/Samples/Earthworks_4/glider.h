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

struct rvPackedGlider
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    unsigned int e;
    unsigned int f;
};

struct rvB_Glider
{
    rvPackedGlider pack();
    int     type = 0;
    bool    startBit = false;
    float3  position;
    int     material;
    float   anim_blend;
    float2  uv;
    float3  bitangent;
    float3  tangent;
    float   radius;

    float4  lightCone;
    float   lightDepth;
    unsigned char ao = 255;
    unsigned char shadow = 255;
    unsigned char albedoScale = 255;
    unsigned char translucencyScale = 255;
};




class _airParticle
{
public:
    float3 pos = float3(0, 0, 0);
    float3 vel = float3(0, 0, 0);
    float3 dVel = float3(0, 0, 0);
    float3 acell = float3(0, 0, 0);
    float r = 1.f;
    float volume = 1.f;
    float volRel = 1.f;
    float volNew = 1.f;
    float mass = 1.f;
    std::array<uint, 16>    space;
    int firstEmpty = -1;
    float firstEmptyW = 1.f;;
};

class _airSim
{
public:
    std::vector<_airParticle>   particles;
    void update(float _dt);
    void pack();
    void setup();

    rvB_Glider ribbon[1024 * 1024];
    rvPackedGlider packedRibbons[1024 * 1024];
    int ribbonCount = 0;
    bool changed = false;
};



class _bezierGlider
{
public:
    void set(float3 a, float3 b, float3 c, float3 d) {
        P0 = a;
        P1 = b;
        P2 = c;
        P3 = d;
    }
    void    solveArcLenght();
    float3  pos(float _s);
    void    renderGui(Gui* mpGui);

    glm::vec3 P0 = float3(0, 0, 0);
    glm::vec3 P1 = float3(2.5, 0, 0);
    glm::vec3 P2 = float3(9, 0, 0);
    glm::vec3 P3 = float3(10, -4, 0);
    float arcLength;

#define archive_float3g(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
    template<class Archive>
    void serialize(Archive& archive)
    {
        archive_float3g(P0);
        archive_float3g(P1);
        archive_float3g(P2);
        archive_float3g(P3);
    }
};



struct _cell
{
    float volume = 0;
    float inlet_pressure = 0;
    float pressure = 0;
    float old_pressure = 0;
    float reynolds = 0;
    float cD;
    float chordLength;
    float AoA;
    float AoA_lead;
    float AoA_trail; //???
    float brakeSetting;

    float AoBrake;
    float ragdoll;

    float3 front;
    float3 right;
    float3 up;
    float3 v;
    float rho;
    //float windspeed;
    float3 flow_direction;
    float cellVelocity;
};



struct _constraint
{
    _constraint(uint _a, uint _b, uint _cell, float _l, float _sa, float _sb, float _sc) : idx_0(_a), idx_1(_b),
        cell(_cell), l(_l), tensileStiff(_sa), compressStiff(_sb), pressureStiff(_sc){ l = __max(0.01, l); }

    _constraint(uint _a, uint _b, uint _cell, float _l, float3 _s) : idx_0(_a), idx_1(_b),
        cell(_cell), l(_l), tensileStiff(_s.x), compressStiff(_s.y), pressureStiff(_s.z) {
        l = __max(0.01, l);
    }

    uint    idx_0;
    uint    idx_1;
    uint    cell;              // for pressure values
    float   l;
    float   tensileStiff = 1.0f;
    float   compressStiff = 0.01f;
    float   pressureStiff = 0.1f;
};



class _lineBuilder
{
public:
    uint calcVerts(float _spacing);
    void addVerts(std::vector<float3>& _x, std::vector<float>& _w, uint& _idx, uint _parentidx, float _spacing);
    void set(std::string _n, float _l, float _r, float3 _color, int2 _attach = uint2(0, 0), float3 _p = float3(0, 0, 0));
    _lineBuilder& pushLine(std::string _n, float _l, float _d, float3 _color, int2 _attach = uint2(0, 0), float3 _dir = float3(0, 0, 0), int _segments = 1);
    void mirror(int _span);
    float3 averageEndpoint(float3 _start);
    void solveLayout(float3 _start, uint _chord);
    void addRibbon(rvB_Glider*_r, std::vector<float3> &_x, int & _c);

    void renderGui(char *search, int tab = -1);

    void wingLoading(std::vector<float3>& _x, std::vector<float3>& _n);
    float maxT(float _r, float _dT);
    void windAndTension(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float3>& _n, std::vector<float3>& _v, float3 _wind, float _dtSquare);
    void solveUp(std::vector<float3>& _x, bool _lastRound, float _ratio = 1.f, float _w0 = 0.f);

    _lineBuilder* getLine(std::string _s);

    std::string name;
    float   length = 1.f;
    float   diameter = 0.04f;
    float3  color;
    std::vector<_lineBuilder> children;
    int2    attachment;                 // (span, coord)        only used if no children to attach to wing
    int     wingIdx;

    uint    parent_Idx;                // points to parent vertex, vetex[0]
    uint    start_Idx;                 // points to vertex[1]
    uint    end_Idx;
    uint    numSegments;               // number of segments in 

    float3 endPos;

    bool    isLineEnd = false;            // is this attached to the wing
    bool    isCarabiner = false;            // is this attached to the wing

    // tensile weight distribution
    float   maxTencileForce;
    float3  tencileVector;
    float   tencileForce;
    float  tencileRequest;
    float straightRatio;

    // new tensile
    float t_ratio;      // ratio of
    float t_combined;
    float stretchRatio;

    // New new new
    float rho_Line;
    float f_Line;
    float rho_End;
    float m_Line;
    float m_End;
};



class _lineBuilder_FILE
{
public:
    _lineBuilder_FILE& pushLine(std::string _name, float _length, float _diameter, int2 _attach = uint2(0, 0), int _segments = 1);
    void generate(std::vector<float3>& _x, int chordSize, _lineBuilder &line);

    std::string name;
    float   length = 1.f;
    float   diameter_mm = 1.f;
    int2    attachment;                 // (span, coord)        only used if no children to attach to wing
    int     segments;

    std::vector<_lineBuilder_FILE> children;
    

//private:
#define archive_f3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
#define archive_i2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(CEREAL_NVP(name));
        archive(CEREAL_NVP(length));
        archive(CEREAL_NVP(diameter_mm));
        archive_i2(attachment);
        archive(CEREAL_NVP(segments));

        archive(CEREAL_NVP(children));
    }
};


class wingShape
{
public:
    void load(std::string _root);
    void load(std::string _root, std::string _path);
    void save();

    _bezierGlider leadingEdge;
    _bezierGlider trailingEdge;
    _bezierGlider curve;

    uint spanSize = 50;
    uint chordSize = 25;
    float maxChord = 2.97f;
    float wingWeight = 5.0f;        //kg

    std::vector<float> chordSpacing;    // for half span
    std::string wingName = "naca4415";
    std::string spacingFile = "NOVA_AONIC.spacing.txt";

    std::vector<int> linebraceSpan;
    std::vector<int> linebraceChord;

private:
    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(CEREAL_NVP(leadingEdge));
        archive(CEREAL_NVP(trailingEdge));
        archive(CEREAL_NVP(curve));

        archive(CEREAL_NVP(spanSize));
        archive(CEREAL_NVP(chordSize));
        archive(CEREAL_NVP(maxChord));
        archive(CEREAL_NVP(wingWeight));

        archive(CEREAL_NVP(wingName));
        archive(CEREAL_NVP(chordSpacing));
        //archive(CEREAL_NVP(spacingFile));

        archive(CEREAL_NVP(linebraceSpan));
        archive(CEREAL_NVP(linebraceChord));
    }
};


/*  Current flying setup */

struct _constraintSetup
{
    float3 chord = float3(1.f, 0.f, 0.0f);              // pull, push, pressure
    float3 chord_verticals = float3(1.f, 0.f, 0.0f);     // pull, push, pressure
    float3 chord_diagonals = float3(0.7f, 0.f, 0.0f);    // pull, push, pressure
    // ??? nose stiffner
    float3 leadingEdge = float3(0.f, 0.6f, 0.3f);    // pull, push, pressure
    float3 trailingEdge = float3(0.f, 0.0f, 0.3f);    // pull, push, pressure

    float3 span = float3(0.7f, 0.1f, 0.2f);
    float3 surface = float3(.5f, 0.f, 0.01f);
    // ??? trailing edge span stiffner

    float3 pressure_volume = float3(0.f, 0.f, 0.2f);

    float3 line_bracing = float3(1.f, 0.f, 0.f);
};


/*
struct _constraintSetup
{
    float3 chord = float3(1.f, 0.f, 0.0f);              // pull, push, pressure
    float3 chord_verticals = float3(1.f, 0.f, 0.0f);     // pull, push, pressure
    float3 chord_diagonals = float3(0.7f, 0.f, 0.0f);    // pull, push, pressure
    // ??? nose stiffner
    float3 leadingEdge = float3(0.f, 0.6f, 0.0f);    // pull, push, pressure
    float3 trailingEdge = float3(0.f, 0.0f, 0.0f);    // pull, push, pressure

    float3 span = float3(0.7f, 0.0f, 0.0f);
    float3 surface = float3(.5f, 0.f, 0.0f);
    // ??? trailing edge span stiffner

    float3 pressure_volume = float3(0.f, 0.f, 0.2f);

    float3 line_bracing = float3(1.f, 0.f, 0.f);
};*/


class _gliderBuilder
{
public:
    void buildWing();
    void builWingConstraints();
    void buildLinesFILE_NOVA_AONIC_medium();
    void buildLines();
    void generateLines();
    void visualsPack(rvB_Glider*ribbon, rvPackedGlider*packed, int &count, bool &changed);
    uint index(uint _s, uint _c) { return (_s * chordSize) + _c; }               // index of this vertex
    uint4 crossIdx(int _s, int _c);
    uint4 quadIdx(int _s, int _c);

    void setxfoilDir(std::string dir) { xfoilDir = dir; }
    void buildCp();
    void analizeCp(std::string name, int idx);
    std::string xfoilDir;
    
    std::vector<std::vector<float>> Cp;
    float CPbrakes[6][11][25];              // F Aoa Vtx  own class with lerp lookup


    // wingShape code, includes saving out files for
    void xfoil_shape(std::string _name);
    void xfoil_createFlaps(float _dst, std::string _name);
    void xfoil_simplifyWing();
    void xfoil_buildCP(int _flaps, int _aoa, std::string _name);
    std::string wingName = "naca4415";
    std::vector<float2> wingshapeFull;
    std::vector<float2> wingshape;
    uint wingIdx[25];
    uint wingIdxB[25];
    float cp_temp[6][36][25];

    // buolding
    wingShape shape;
    _constraintSetup cnst;

    
    //_bezierGlider leadingEdge;
    //_bezierGlider trailingEdge;
    //_bezierGlider curve;
    _lineBuilder linesLeft;
    _lineBuilder linesRight;

    uint spanSize = 50;
    uint chordSize = 25;
    //float maxChord = 2.97f;
    //float wingWeight = 5.0f;        //kg

    std::vector<float3> x;              //position      {_gliderBuilder}
    std::vector<float>  w;              // weight = 1/mass      {_gliderBuilder}
    //..std::vector<float>  cdA;             // NORT USED
    std::vector<uint4>  cross;          // Index of cross verticies for normal calculation      {_gliderBuilder}
    std::vector<float>  area;

    std::vector<_constraint>    constraints;

    // reference values
    float flatSpan;
    float flatArea;
    float totalArea;
    std::vector<float> cellWidth;
    std::vector<float> cellChord;
    std::vector<float3> ribNorm;

private:
    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(leadingEdge));
        _archive(CEREAL_NVP(trailingEdge));
        _archive(CEREAL_NVP(curve));

        _archive(CEREAL_NVP(linesLeft));
        _archive(CEREAL_NVP(linesRight));
    }
};
CEREAL_CLASS_VERSION(_gliderBuilder, 100);









struct _gliderwingVertex
{
    float3 pos;
    uint material;
    float3 normal;
    float something;
    float3 uv;
    float somethignesle;
};






/*  Look at ribbons for packing, especailly normals
    recon I can get this into 4 uints - damn awesome, but needs individual buildings*/
struct _buildingVertex
{
    float3 pos;
    uint material;
    float3 normal;
    float something;
    float3 uv;
    float somethignesle;
};

enum edgeType { internal, edgefloor, leading, roof, trailing };
struct _edge
{
    float3 normal;
    float length;
    edgeType type;      // internal floot, roof, prev, next
};

struct _faceInfo
{
    float3  normal;
    float   planeW;
    bool    isFlat;
    uint    type;   // floot, wall, roof
    uint    index;  // for walls and roofs, group together
    uint    vertIndex[4];

    _edge edges[4];
};

struct _wallinfo
{
    float3 normal;
    float3 right;
    float W;
    std::vector<uint> faces;
    bool isFlat;
    float length;
    float height;
    uint stories = 1;
};

struct _swissBuildings
{
    aiVector3D reproject(aiVector3D v);
    glm::dvec3 reproject(double x, double y, double z);
    glm::dvec3 reprojectLL(double latt, double lon, double alt);
    void process(std::string _name);
    void processWallRoof(std::string _name);
    void processGeoJSON(std::string _path, std::string _name);
    void exportBuilding(std::string _path, std::vector<float3> _v, std::vector<unsigned int>f);
    void findWalls();
    void findWallEdges(_wallinfo &_wall);

    

    uint numBuildings = 0;
    std::vector<float3> verts;
    std::vector<glm::dvec3> vertsDouble;
    std::vector<uint> faces;
    std::vector <_faceInfo> faceInfo;
    std::vector <_wallinfo> wallInfo;

    std::vector<float3> all_verts;
    std::vector<uint>   all_faces;
};


struct _windTerrain
{
    float height[4096][4096];
    float3 norm[4096][4096];   // temp
    float3 wind[4096][4096];
    float3 windTop[4096][4096];
    float3 windTemp[4096][4096];
    unsigned char image[4096][4096];
    unsigned char Nc[4096][4096][3];
    // plus some blurrign for higher


    void load(std::string filename, float3 _wind);
    void setWind(float3 _wind = float3(-1, 0, 0));
    void normalizeWind();
    void normalizeWindLinear();
    float3 normProj(float3 w, float3 half);
    void solveWind();
    void saveRaw();

    float3 N(float2 _uv);
    void loadRaw();
    float H(float3 _pos);
    float3 W(float3 _pos);

    float Hrel(float2 _pos);
    //float3 Wrel(float2 _pos);
};





class _lineRuntime
{
public:
    void addRibbon(rvB_Glider* _r, std::vector<float3>& _x, int& _c);
    void renderGui(char* search, int tab = -1);

    void wingLoading(std::vector<float3>& _x, std::vector<float3>& _n);
    float maxT(float _r, float _dT);
    void windAndTension(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float3>& _n, std::vector<float3>& _v, float3 _wind, float _dtSquare);
    void solveUp(std::vector<float3>& _x, bool _lastRound, float _ratio = 1.f, float _w0 = 0.f);

    _lineRuntime* getLine(std::string _s);

    // This parth is in CEREAL
    std::string name;
    float   length = 1.f;
    float   diameter = 0.04f;
    float3  color;
    std::vector<_lineRuntime> children;
    //int2    attachment;                 // (span, coord)        only used if no children to attach to wing
    int     wingIdx;
    bool    isLineEnd = false;            // is this attached to the wing
    bool    isCarabiner = false;            // is this attached to the wing

    // add an x, v, w here for end point
//    uint    parent_Idx;                // points to parent vertex, vetex[0]
//    uint    start_Idx;                 // points to vertex[1]
//    uint    end_Idx;
//    uint    numSegments;               // number of segments in 

//    float3 endPos;

    

    // runtime only components ---------------------------------------------
    // tensile weight distribution
    float   maxTencileForce;
    float3  tencileVector;
    float   tencileForce;
    float  tencileRequest;
    float straightRatio;

    // new tensile
    float t_ratio;      // ratio of
    float t_combined;
    float stretchRatio;

    // New new new
    float rho_Line;
    float f_Line;
    float rho_End;
    float m_Line;
    float m_End;
};


class _cp
{
public:
    void load(std::string _name);
    float get(float _brake, float aoa, uint _chord);
    float cp[6][36][25];
};


struct _rib
{
    float4 plane;
    std::vector<glm::ivec2> edges;
    std::vector<glm::ivec2> outer_edge;
    std::vector<int> outer_idx;
    std::vector<float3> outerV;
    std::vector<float3> lower;
    std::vector<float3> ribVerts;

    float circumference;
    float3 leadEdge;
    float3 trailEdge;
    float chord;
    float3 front;
    float3 up;
};

class _gliderImporter
{
public:
    void processRib();
    void placeRibVerts();

    std::string rib_file = "E:\\Advance\\OmegaXA5_23_Johan_A01\\OmegaXA5_23_Johan_A01\\LowPoly_OmegaXA5_23_Johan_A01_RibSurfaces.obj";
    std::string semirib_file;
    std::string panels_file;
    std::string diagonals_file;
    std::string line_file;
    std::string vecs_file;

    // rib processing
    std::vector<float3> rib_verts;
    std::vector<_rib> half_ribs;
    bool cmp_rib_V(float3 a, float3 b) { return (a.x < b.x); }
    bool fixedRibcount = false;
    int ribVertCount = 50;
    float ribSpacing = 0.15f;    // 10 cm
    float trailingBias = 1.5f;  // on the last 3de
    float leadBias = 2.5f;      // on the front 3de

private:
    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(rib_file));
        archive(CEREAL_NVP(semirib_file));
        archive(CEREAL_NVP(panels_file));
        archive(CEREAL_NVP(diagonals_file));
        archive(CEREAL_NVP(line_file));
        archive(CEREAL_NVP(vecs_file));

        archive(CEREAL_NVP(fixedRibcount));
    }
};
CEREAL_CLASS_VERSION(_gliderImporter, 100);






class _gliderRuntime
{
public:
    void renderGui(Gui* mpGui);
    void renderHUD(Gui* mpGui, float2 _screen);
    void renderDebug(Gui* mpGui, float2 _screen);
    void renderImport(Gui* mpGui, float2 _screen);
    bool importGui = false;
    _gliderImporter importer;

    void setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints);
    bool requestRestart = false;
    void setupLines();
    void solve_air(float _dT, bool _left);
    void solve_aoa(float _dT, bool _left);
    void solve_pressure(float _dT, bool _left);
    void solve_surface(float _dT, bool _left);

    void solve_PRE(float _dT);
    void solve_linesTensions(float _dT);
    void solve_triangle(float _dT);
    void solve_rectanglePilot(float _dT);

    void solve_wing(float _dT, bool _left);

    void solve_POST(float _dT);
    void movingROOT();
    void eye_andCamera();

    void solve(float _dT);

//    void constraintThread_a();
//    void constraintThread_b();
//    void lineThread_left();
//    void lineThread_right();
    void packWing(float3 pos, float3 norm, float3 uv);
    void pack_canopy();
    void pack_lines();
    void pack_feedback();
    void pack();
    void load();
    void save();
    uint index(uint _s, uint _c) { return _s * chordSize + _c; } 
    void solveConstraint(uint _i, float _step);

    void setJoystick();

    void exportGliderShape();

    // interaction with wind
    float3 pilotPos() { return ROOT + x[spanSize * chordSize]; }
    float3 wingPos(uint span) { return ROOT + x[span * chordSize + 12]; }
    float3 pilotWind = {0, 0, 0};
    std::array<float3, 3> wingWind = { float3(0, 0, 0), float3(0, 0, 0) , float3(0, 0, 0) };
    void setPilotWind(float3 _v) { pilotWind = _v; }
    void setWingWind(float3 _v0, float3 _v1, float3 _v2) { wingWind[0] = _v0; wingWind[1] = _v1; wingWind[2] = _v2;  }

    float3 pathRecord[5 * 60 * 60 * 10];    // 5 hours at 10 per second
    uint recordIndex = 0;

    void setWind(std::string file, float3 _wind) { windTerrain.load(file, _wind); }
    void loadWind() { windTerrain.loadRaw(); }
    void setxfoilDir(std::string dir) { xfoilDir = dir; }
    void setWing(std::string _name) { wingName = _name; }
    std::string xfoilDir;
    std::string wingName = "naca4415";
    _cp Pressure;


    std::array<float, 10000> sound_500Hz;  // 20 seconds * 500Hz
    //std::array<float, 100000> sound_5kHz;  // 20 seconds * 500Hz
    uint soundPtr = 0;
    std::vector<float> soundLowPass;

    // build
    rvB_Glider ribbon[1024 * 1024];
    rvPackedGlider packedRibbons[1024 * 1024];
    int ribbonCount = 0;
    bool changed = false;

    int wingVertexCount = 0;
    _gliderwingVertex packedWing[16384];

    // Play
    float weightShift = 0.5f;
    float weightLength = 0.4f;
    bool symmetry = true;
    _lineBuilder* pBrakeLeft = nullptr;
    _lineBuilder* pBrakeRight = nullptr;

    _lineBuilder* pEarsLeft = nullptr;
    _lineBuilder* pEarsRight = nullptr;

    _lineBuilder* pSLeft = nullptr;
    _lineBuilder* pSRight = nullptr;
    float maxBrake, maxEars, maxS;

    struct _xpdb
    {
        float3  x;
        float   w;

        float3  xPrev;

        float3  v;
        float3  normal;
        float3  tangent;

        uint4   neighbours;
    };

    std::vector<float3> x;              //position      {_gliderBuilder}
    std::vector<float>  w;              // weight = 1/mass      {_gliderBuilder}
    //std::vector<float>  cd;             // drag - for lines etc      {_gliderBuilder}
    std::vector<uint4>  cross;          // Index of cross verticies for normal calculation      {_gliderBuilder}
    std::vector<float3> v;              // velocity
    std::vector<float3> n;              // normal - scaled with area
    std::vector<float3> t;              // tangent - scaled with area
    std::vector<float3> cp_feedback;     

    std::vector<float3> x_old;
    

    std::vector<_constraint>    constraints;

    std::vector <_cell>     cells;

    std::vector<std::vector<float>> Cp;// deprecated
    float CPbrakes[6][11][25];              // deprecated

    _lineBuilder linesLeft;
    _lineBuilder linesRight;
    //float3 sumLineEndings;

    uint spanSize = 50;
    uint chordSize = 25;

    float3 EyeLocal = float3(0, 0, 0);
    float3 ROOT = float3(0, 0, 0);
    float3 OFFSET = float3(0, 0, 0);
    float3 sumFwing;
    float3 sumDragWing;

    float3 tensionCarabinerLeft;
    float3 tensionCarabinerRight;
    float3 pilotDrag;
    float3 lineDrag;

    float weightLines;
    float weightPilot;
    float weightWing;

    float3 v_oldRoot;
    float3 a_root;
    float vBody = 0;

    // stepping
    float relAlt;
    float relDist;
    float glideRatio;
    uint runcount = 10000000;
    uint frameCnt = 0;
    bool singlestep = false;
    void playpause() {
        if (runcount == 0 && singlestep) runcount = 1;
        else if (runcount == 0 && !singlestep) runcount = 10000000;
        else runcount = 0;
    }
    void runstep() {
        singlestep = !singlestep;
    }

    float CPU_usage = 0;

    float pilotYaw = 0.f;
    float cameraYaw[3] = { 0.f, 0.f, 0.f };
    float cameraPitch[3] = { 0.f, 0.f, 0.f };
    float cameraDistance[3] = { 0.f, 3.f, 3.f };
    float3 cameraUp = float3(0, 1, 0);
    uint cameraType = 0;

    float rumbleLeft = 0.f;
    float rumbleLeftAVS = 0.f;
    float rumbleRight = 0.f;
    float rumbleRightAVS = 0.f;

    float3 camUp;
    float3 camRight;
    float3 camDir;
    float3 camPos;

    BYTE leftTrigger;
    BYTE rightTrigger;


    float time_ms_Aero;
    float time_ms_Pre;
    float time_ms_Wing;
    float time_ms_Post;

    _windTerrain windTerrain;
         
    float energy, energyOld, d_energy;


    
private:
    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
    }
};
CEREAL_CLASS_VERSION(_gliderRuntime, 100);



