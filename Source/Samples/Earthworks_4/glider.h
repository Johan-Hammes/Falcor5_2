#pragma once
#include "Falcor.h"

#include "../../external/cereal-master/include/cereal/cereal.hpp"
#include "../../external/cereal-master/include/cereal/archives/binary.hpp"
#include "../../external/cereal-master/include/cereal/archives/json.hpp"
#include "../../external/cereal-master/include/cereal/archives/xml.hpp"
#include <fstream>

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

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float3(P0);
        archive_float3(P1);
        archive_float3(P2);
        archive_float3(P3);
    }
};
CEREAL_CLASS_VERSION(_bezierGlider, 100);


struct _cell
{
    float volume = 0;
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
    float3 up;
    float3 v;
    float rho;
    float windspeed;
    float3 rel_wind;
    float cellVelocity;
};



struct _constraint
{
    _constraint(uint _a, uint _b, uint _cell, float _l, float _sa, float _sb, float _sc) : idx_0(_a), idx_1(_b),
        cell(_cell), l(_l), tensileStiff(_sa), compressStiff(_sb), pressureStiff(_sc){ l = __max(0.01, l); }

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
    void addVerts(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float>& _cd, uint& _idx, uint _parentidx, float _spacing);
    void set(std::string _n, float _l, float _r, float3 _color, int2 _attach = uint2(0, 0), float3 _p = float3(0, 0, 0));
    _lineBuilder& pushLine(std::string _n, float _l, float _d, float3 _color, int2 _attach = uint2(0, 0), float3 _dir = float3(0, 0, 0), int _segments = 1);
    void mirror(int _span);
    float3 averageEndpoint(float3 _start);
    void solveLayout(float3 _start, uint _chord);
    void addRibbon(rvB_Glider*_r, std::vector<float3> &_x, int & _c);

    void renderGui(int tab = -1);

    void wingTencile(std::vector<float3>& _x, std::vector<float3>& _n);
    float maxT(float _r, float _dT);
    void windAndTension(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float3>& _n, std::vector<float3>& _v, float3 _wind, float _dtSquare);
    float3 tensionDown(std::vector<float3>& _x, std::vector<float3>& _n);
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

    // tensdile wight distribution
    //float   tension;
    float   maxTencileForce;
    float3  tencileVector;
    float   tencileForce;
    float  tencileRequest;
    float3  wingForce;
    float straightRatio;

    // new tensile
    float t_ratio;      // ratio of
    float t_combined;

    // new new tensile
    float stretchLength;
    float stretchRatio;

    // New new new
    float rho_Line;
    float f_Line;
    float rho_End;
    float m_Line;
    float m_End;
};



class _gliderBuilder
{
public:
    void buildWing();
    void builWingConstraints();
    void buildLines();
    void generateLines();
    void builLineConstraints();
    void solveLines();
    void renderGui(Gui* mpGui);
    void visualsPack(rvB_Glider*ribbon, rvPackedGlider*packed, int &count, bool &changed);
    uint index(uint _s, uint _c);               // index of this vertex
    uint4 crossIdx(int _s, int _c);
    uint4 quadIdx(int _s, int _c);

    std::vector<float3> wingshape;
    std::vector<std::vector<float>> Cp;
    float CPbrakes[6][11][25];
    //float Cp[15][23];
    void buildCp();
    void analizeCp(std::string name, int idx);

    _bezierGlider leadingEdge;
    _bezierGlider trailingEdge;
    _bezierGlider curve;
    _lineBuilder linesLeft;
    _lineBuilder linesRight;

    uint spanSize = 50;
    uint chordSize = 25;
    float maxChord = 2.97f;
    float wingWeight = 5.0f;        //kg

    std::vector<float3> x;              //position      {_gliderBuilder}
    std::vector<float>  w;              // weight = 1/mass      {_gliderBuilder}
    std::vector<float>  cdA;             // drag - for lines etc      {_gliderBuilder}
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










class _gliderRuntime
{
public:
    void renderGui(Gui* mpGui);
    void setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<float>& _cd, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints);
    void solve(float _dT);
    void pack(rvB_Glider* ribbon, rvPackedGlider* packed, int& count, bool& changed);
    void load();
    void save();
    uint index(uint _s, uint _c);               // index of this vertex
    void solveConstraint(uint _i, float _step);

    // build
    rvB_Glider ribbon[1024 * 1024];
    rvPackedGlider packedRibbons[1024 * 1024];
    int ribbonCount = 0;
    bool changed = false;

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


    std::vector<float3> x;              //position      {_gliderBuilder}
    std::vector<float>  w;              // weight = 1/mass      {_gliderBuilder}
    std::vector<float>  cd;             // drag - for lines etc      {_gliderBuilder}
    std::vector<uint4>  cross;          // Index of cross verticies for normal calculation      {_gliderBuilder}
    std::vector<float3> l;              // lamda
    std::vector<float3> v;              // velocity
    std::vector<float3> n;              // normal - scaled with area
    std::vector<float3> t;              // tangent - scaled with area

    std::vector<float3> x_old;
    std::vector<float3> l_old;

    std::vector<_constraint>    constraints;

    std::vector <_cell>     cells;

    std::vector<std::vector<float>> Cp;
    float CPbrakes[6][11][25];

    _lineBuilder linesLeft;
    _lineBuilder linesRight;
    float3 sumLineEndings;

    uint spanSize = 50;
    uint chordSize = 25;

    float3 ROOT = float3(0, 0, 0);
    float3 OFFSET = float3(0, 0, 0);
    float3 sumFwing;
    float3 sumFwing_F;
    float3 sumFwing_A;
    float3 sumFwing_B;
    float3 sumFwing_C;
    float3 sumDragWing;
    //float3 tempAForce[100];

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
    float3 wingV;
    float3 wingVold;
    float3 wingA;
    float root_AG01;
    // stepping
    uint runcount = 0;
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

    float pilotYaw;

private:
    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
    }
};
CEREAL_CLASS_VERSION(_gliderRuntime, 100);



