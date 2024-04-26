#pragma once
#include "Falcor.h"

#include "../../external/cereal-master/include/cereal/cereal.hpp"
#include "../../external/cereal-master/include/cereal/archives/binary.hpp"
#include "../../external/cereal-master/include/cereal/archives/json.hpp"
#include "../../external/cereal-master/include/cereal/archives/xml.hpp"
#include "../../external/cereal-master/include/cereal/types/vector.hpp"
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
    float windspeed;
    float3 rel_wind;
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


struct _constraintSetup
{
    float3 chord = float3(1.f, 0.f, 0.5f);              // pull, push, pressure
    float3 chord_verticals = float3(1.f, 0.f, 0.f);     // pull, push, pressure
    float3 chord_diagonals = float3(0.7f, 0.f, 0.f);    // pull, push, pressure
    // ??? nose stiffner

    float3 span = float3(0.7f, 0.f, 0.7f);
    float3 surface = float3(.5f, 0.f, 0.5f);
    // ??? trailing edge span stiffner

    float3 pressure_volume = float3(0.f, 0.f, 0.4f);

    float3 line_bracing = float3(1.f, 0.f, 0.f);
};


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




class _cp
{
public:
    void load(std::string _name);
    float get(float _brake, float aoa, uint _chord);
    float cp[6][36][25];
};





class _gliderRuntime
{
public:
    void renderGui(Gui* mpGui);
    void setup(std::vector<float3>& _x, std::vector<float>& _w, std::vector<uint4>& _cross, uint _span, uint _chord, std::vector<_constraint>& _constraints);
    void solve(float _dT);
    void pack_canopy();
    void pack_lines();
    void pack_feedback();
    void pack();
    void load();
    void save();
    uint index(uint _s, uint _c) { return _s * chordSize + _c; } 
    void solveConstraint(uint _i, float _step);

    void setxfoilDir(std::string dir) { xfoilDir = dir; }
    std::string xfoilDir;
    std::string wingName = "naca4415";
    _cp Pressure;

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

    std::vector<float3> x_old;
    

    std::vector<_constraint>    constraints;

    std::vector <_cell>     cells;

    std::vector<std::vector<float>> Cp;
    float CPbrakes[6][11][25];

    _lineBuilder linesLeft;
    _lineBuilder linesRight;
    //float3 sumLineEndings;

    uint spanSize = 50;
    uint chordSize = 25;

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



