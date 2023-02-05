#pragma once


#include "Falcor.h"
#include "terrafector.h"
#include "roads_intersection.h"

using namespace Falcor;

#define archive_float2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
#define archive_float3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
#define archive_float4(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z)); archive(CEREAL_NVP(v.w));}



class roadMaterialLayer {
public:
    roadMaterialLayer() { ; }
    virtual ~roadMaterialLayer() { ; }

    static std::string rootFolder;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        archive(CEREAL_NVP(displayName));
        archive(CEREAL_NVP(bezierIndex));
        archive(CEREAL_NVP(sideA));
        archive(CEREAL_NVP(offsetA));
        archive(CEREAL_NVP(sideB));
        archive(CEREAL_NVP(offsetB));
        archive(CEREAL_NVP(material));
    }

public:
    std::string		displayName = "please change me";
    uint			bezierIndex = 0;
    uint			sideA = 0;
    float			offsetA = 0.0f;
    uint			sideB = 0;
    float			offsetB = 0.0f;
    std::string		material;
    int				materialIndex = -1;
};
CEREAL_CLASS_VERSION(roadMaterialLayer, 100);



struct roadMaterialGroup
{
    std::string				displayName = "please change me";
    std::string				relativePath;
    Texture::SharedPtr		thumbnail = nullptr;

    std::vector<roadMaterialLayer> layers;
    void import(std::string _path);

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        archive(CEREAL_NVP(displayName));
        archive(CEREAL_NVP(relativePath));
        archive(CEREAL_NVP(layers));
    }
};
CEREAL_CLASS_VERSION(roadMaterialGroup, 100);



class roadMaterialCache {
public:
    void renderGui(Gui* _gui);
    uint find_insert_material(const std::string _path);
    void reloadMaterials();

    std::vector<roadMaterialGroup>	materialVector;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        archive(CEREAL_NVP(materialVector));
    }
};
CEREAL_CLASS_VERSION(roadMaterialCache, 100);







enum bezier_edge { center, outside };
struct bezierLayer
{
    bezierLayer() { ; }
    bezierLayer(bezier_edge inner, bezier_edge outer, uint _material, uint bezierIndex, float w0, float w1, bool startSeg = false, bool endSeg = false);
    uint A;			// flags, material, index [2][14][16]			combine with rootindex in constant buffer
    uint B;			// w0, w1 [4][14][14]
};



struct testBezierReturn {
    bool bHit = false;
    uint index;
    float u, v;			// uv for index segment, translated vy UV mapping
    uint bezier;
    uint bezierCacheIndex;
    float s, t;
    int t_idx;
};



struct plane_2d {
    float dst;
    glm::vec2 norm;		// test vec2, should work Y = 0

    void frompoints(glm::vec3 A, glm::vec3 B);
    float distance(glm::vec2 P);
};



struct cachePoint {
    float3 A;
    float dA;
    float3 B;
    float dB;

    plane_2d planeTangent;
    plane_2d planePerp;
};

#define numBezierCache 16
#define bezierCacheSize 10



// a semi-random cahche since its faster the LRU
struct bezierSegment {
    float3 A;		// FIXME float4 for UV's
    float radius;
    float3 B;
    float optimalShift;	//8

    plane_2d planeTangent;
    plane_2d planeInner;
    plane_2d planeOuter;	//17

    float distance;	// For AI - so from the start I guess
    float length;	// in teh middle, avergawe of left and right side
    float camber;
    float curveRadius;

    // EXTRA STUFF
    float3 optimalPos;
    float optimalTemp;
};



struct cacheItem {
    uint counter = 999999;		// for LRU-ish, start big
    int bezierIndex = -1;	// points back to 
    bezierSegment points[numBezierCache + 1];
};



struct physicsBezierLayer {
    bezier_edge edge[2];
    float offset[2];

    uint material;
};



struct physicsBezier {
    physicsBezier() { ; }
    physicsBezier(splinePoint a, splinePoint b, bool bRight, uint _guid, uint _index, bool _fullWidth = false);
    void addLayer(bezier_edge inner, bezier_edge outer, uint material, float w0, float w1);

    void binary_import(FILE* file);
    void binary_export(FILE* file);

    glm::vec2 center;
    float radius;
    plane_2d startPlane;
    plane_2d endPlane;
    int cacheIndex = -1;	// -1 if not cached
    bool bRighthand;	// this comes from a right hand side of the road - needed for editing - not currently exported

    glm::vec4 bezier[2][4];
    std::vector<physicsBezierLayer> layers;

    // editor info only
    uint roadGUID;
    uint index;
};




struct bezierCache {

    uint findEmpty();
    void solveStats();
    void cacheSpline(physicsBezier* pBez, uint cacheSlot);
    void increment();
    void clear();

    std::array<cacheItem, bezierCacheSize> cache;
    uint numUsed;
    uint numFree;
};




struct bezierOneIntersection {

    bezierOneIntersection() { ; }
    bezierOneIntersection(glm::vec2 P, uint boundingIndex);

    void updatePosition(glm::vec2 P, float distanceTraveled);

    glm::vec2 pos;
    bool bHit = false;
    float distanceTillNextSearch = 0;		// to improve the wide searches if we are far away from everything - set during wide search
    int boundIndex = -1;
    int cacheIndex = -1;
    int t_idx = 0;

    glm::vec2 UV;
    float d0;		// distances in from the edge for V calculation  SHIT NAQME RETHINK
    float d1;
    float bezierHeight;	// before displacement maps

    bool bRighthand;
    uint roadGUID;
    uint index;
    uint lane;		// set later by the road
    float a;
    float W;
};



struct bezierIntersection
{
    void updatePosition(glm::vec2 P);

    glm::vec2 pos;
    uint cellHash = 0;
    std::vector<bezierOneIntersection> beziers;

    // and this is what we are trying to solve for
    //bool bHit = false;
    uint numHits = 0;
    float height;
    float grip;
    float wetness;

    // stuff to make it useful for editing
    // some is added back by the road
    bool bRightHand;	// editor only
    int lane;
};



struct bezierFastGrid
{
    void allocate(float size, uint numX, uint numY);
    void populateBezier();
    inline uint getHash(glm::vec2 pos);
    std::vector<uint> getBlockLookup(uint hash);
    void insertBezier(glm::vec2 pos, float radius, uint index);

    void binary_import(FILE* file);
    void binary_export(FILE* file);

    float size;
    int2 offset;
    uint nX, nY;	// just assume simmetry aroudn the origin for now
    uint maxHash;

    // temp data for the build
    std::vector<std::vector<uint>> buidData;
};



class ODE_bezier {
public:
    void intersect(bezierIntersection* pI);
    bool intersectBezier(bezierOneIntersection* pI);
    bool testBounds(bezierOneIntersection* pI);
    void blendLayers(bezierIntersection* pI);
    void setGrid(float size, uint numX, uint numY);
    void buildGridFromBezier();
    void clear();

public:
    std::vector<physicsBezier> bezierBounding;
    std::vector<physicsBezier> bezierAI;	// test for Kevin
    bezierFastGrid	gridLookup;
    bezierCache cache;
    bool needsReCache = false;
};



struct aiIntersection {

    void setPos(glm::vec2 _p);
    glm::vec2 pos;
    glm::vec2 dir;
    float delDistance;

    bool bHit = false;

    float dist_Left;
    float dist_Right;
    float	distance;		// how far past the start position we are
    uint	segment;		// current bezier patch we are on
    float	road_angle;	// relative to current tangent vector


    // and then some data describing the road up ahead
    uint numSteps;
    float camber[100];

    float lookahead_curvature;
    float lookahead_camber;
    float lookahead_apex_distance;
    uint lookahead_apex_left;			// none, left, right
    float lookahead_apex_camber;
    float lookahead_apex_curvature;
};



class AI_bezier {
public:
    glm::vec3 startpos;
    glm::vec3 endpos;
    float start;	// in meters I guess
    float end;
    float length;
    uint startIdx;
    uint endIdx;

    uint numBezier;
    int isClosedPath = false;
    float pathLenght = 0.0f;

    std::vector<cubicDouble>	bezierPatches;
    std::vector<bezierSegment>  patchSegments;
    std::vector<bezierSegment>  segments;
    int kevRepeats = 100;
    float kevOutScale = 22;
    float kevInScale = 34;
    int kevSmooth = 5;
    int kevAvsCnt = 0;
    int kecMaxCnt = 9;
    float kevSampleMinSpacing = 5.0f;

    void intersect(aiIntersection* _pAI);
    float rootsearch(glm::vec2 _pos);

    void clear();
    void pushSegment(glm::vec4 bezier[2][4]);	// just cache as well
    void cacheAll();
    void solveStartEnd();


    void exportAI(FILE* file);
    void exportAITXTDEBUG(FILE* file);
    void exportCSV(FILE* file, uint _side);
    void exportGates(FILE* file);

    void save();
    void load(FILE* file);


    // Now excpand it for actual path generation 
    struct road {
        uint roadIndex;
        bool bForward;
    };
    std::vector<road>	roads;

    void setStart(glm::vec3 _start) { startpos = _start; }
    void setEnd(glm::vec3 _end) { endpos = _end; }
    void clearRoads() { roads.clear(); }
    void addRoad(road _road) { roads.push_back(_road); }
};



struct splineTest {
    float3 pos;
    float3 returnPos;
    bool bVertex;
    bool bSegment;
    int index;

    bool bStreetview;
    int streetviewIndex;

    // intersections
    int cornerNum = -1;
    int cornerFlag;	// tangent or point
    bool bCenter;

    bool bCtrl;
    bool bAlt;
    bool bShift;

    // lets see what we can add for lanes
    bool bRight;
    int lane;

    float testDistance;
};



struct splineUV {
    float3 pos;
    float U;
    float V;
    glm::vec3 dU;
    glm::vec3 dV;
    glm::vec3 N;			// ??? maybe textyure dep[endent
    glm::vec3 E1;
    glm::vec3 E2;
};















class intersection;
class roadSection {
public:
    roadSection() { ; }
    virtual ~roadSection() { ; }

    void convertToGPU_Realistic(uint _from = 99999, uint _to = 88888, bool _stylized = false, bool _showMaterials = true);
    void clearSelection();
    void selectAll();
    void newSelection(int index);
    void addSelection(int index);
    void breakLink(bool bStart);
    void setAIOnly(bool AI);


    int GUID;
    int int_GUID_start = -1;
    int int_GUID_end = -1;
    std::vector<splinePoint> points;

    void saveType(int index);
    void loadSelected(int index);
    void loadCompleteRoad();

    void optimizeTangents(int lane);
    void optimizeSpacing(int lane);
    void solveUV(int lane);
    void solveWidthFromLanes();
    void solveEnergyAndLength(int lane, int _min, int _max);
    void solveRoad(int index = -1);
    void solveStart();
    void solveEnd();

    void setCenterline(uint type);

    void pushPoint(float3 p, float3 n, uint _lod);
    void insertPoint(int index, float3 p, float3 n, uint _lod);
    void deletePoint(int index);
    bool testAgainstPoint(splineTest* testdata, bool includeEnd = true);
    float getDistance();

    void findUV(glm::vec3 p, splineUV* uv);
    void findUVTile(glm::vec3 p, glm::vec3 c1, glm::vec3 c2, glm::vec3 c3, glm::vec3 c4, glm::vec3 b1, glm::vec3 b2, glm::vec3 b3, glm::vec3 b4, splineUV* uv);

    std::array<bool, 9> leftLaneInUse;
    std::array<bool, 9> rightLaneInUse;

    int buildQuality = 0;

    static splinePoint lastEditedPoint;
    static std::vector<intersection>* static_global_intersectionList;


    bool isClosedLoop = false;


    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        archive(CEREAL_NVP(GUID));
        archive(CEREAL_NVP(int_GUID_start));
        archive(CEREAL_NVP(int_GUID_end));
        archive(CEREAL_NVP(points));
        archive(CEREAL_NVP(buildQuality));

        if (version >= 101) {
            archive(CEREAL_NVP(isClosedLoop));
        }
    }
};
CEREAL_CLASS_VERSION(roadSection, 101);





class roadNetwork {
public:
    roadNetwork();
    bool init();
    virtual ~roadNetwork() { ; }


    void renderGUI(Gui* _gui);
    void renderGUI_3d(Gui* _gui);
    bool renderpopupGUI(Gui* _gui, roadSection* _road, int _vertex);
    bool renderpopupGUI(Gui* _gui, intersection* _intersection);
    void saveRoadMaterials(roadSection* _road, int _vertex);
    void loadRoadMaterials(roadSection* _road, int _from, int _to);
    void loadRoadMaterialsAll(roadSection* _road, int _from, int _to);
    void loadRoadMaterialsVerge(roadSection* _road, int _from, int _to);
    void loadRoadMaterialsSidewalk(roadSection* _road, int _from, int _to);
    void loadRoadMaterialsAsphalt(roadSection* _road, int _from, int _to);
    void loadRoadMaterialsLines(roadSection* _road, int _from, int _to);
    void loadRoadMaterialsWT(roadSection* _road, int _from, int _to);

    void renderPopupVertexGUI(Gui* mpGui, glm::vec2 _pos, uint _idx);
    void renderPopupSegmentGUI(Gui* mpGui, glm::vec2  _pos, uint _idx);
    bool popupVisible = false;

    void newRoadSpline();
    void newIntersection();



    float getDistance();	// how many km of roads do we have
    int getDone();		// what percentage is at highest quality level

    void updateDynamicRoad();
    void updateAllRoads(bool _forExport = false);

    std::filesystem::path lastUsedFilename;
    void load(uint _version);
    void load(std::filesystem::path _path, uint _version = 101);
    void save();
    void quickSave();
    void exportBinary();
    void exportBridges();
    void exportRoads();

    void TEMP_pushAllMaterial();

    void currentRoadtoAI();
    void exportAI();
    void exportAI_CSV();
    void loadAI();
    bool AI_path_mode = false;
    void ai_clearPath() { pathBezier.clearRoads(); }
    void addRoad();

    // testing and intersection
    void physicsTest(glm::vec3 pos);
    void doSelect(glm::vec3 pos);
    void testHit(glm::vec3 pos);
    void testHitAI(glm::vec3 pos);
    void lanesFromHit();

    // basic editing
    void breakCurrentRoad(uint _index);
    void deleteCurrentRoad();
    void deleteCurrentIntersection();
    bool hasClipboardPoint = false;
    splinePoint clipboardPoint;
    splinePoint lastusedPoint;
    void copyVertex(uint _index);
    void pasteVertexGeometry(uint _index);
    void pasteVertexMaterial(uint _index);
    void pasteVertexAll(uint _index);

    void currentIntersection_findRoads();
    void solveIntersection(bool _solveTangents = false);
    void solve2RoadIntersection();

    std::vector<roadSection> roadSectionsList;
    std::vector<intersection> intersectionList;

    roadSection* currentRoad = nullptr;
    std::vector<int> subSelection;
    intersection* currentIntersection = nullptr;
    roadSection* intersectionSelectedRoad = nullptr;

    static ODE_bezier odeBezier;	// for the physics lookup, so this can build it, save and load binary etc
    static std::vector<cubicDouble>	staticBezierData;
    static std::vector<bezierLayer>	staticIndexData;
    //static std::vector<bezierLayer>	staticIndexDataSolid;
    uint debugNumBezier;
    uint debugNumIndex;

    AI_bezier pathBezier;
    aiIntersection	AIpathIntersect;


    bool isDirty;
    bezierIntersection physicsMouseIntersection;
    bool bHIT;
    int hitRandomFeedbackValue;
    uint hitRoadGuid;
    uint hitRoadIndex;
    uint hitRoadLane;
    bool hitRoadRight;
    float dA, dW;



    std::string rootPath;

    // Bridge display -----------------------------------------------
    /*
    using ModelInstance = ObjectInstance<Model>;
    Scene::SharedPtr					sceneBridges = nullptr;
    SceneRenderer::SharedPtr			rendererSceneBridges = nullptr;
    GraphicsProgram::SharedPtr mpProgramBridges = nullptr;
    GraphicsVars::SharedPtr mpProgramVarsBridges = nullptr;
    std::vector<ModelInstance::SharedPtr> 	bridge_instances;
    std::vector<Model::SharedPtr>	bridge_meshes;
    void reloadBridges();
    void render(RenderContext::SharedPtr pRenderContext, Camera::SharedPtr pCamera);
    */

    // interactive lane editing
    bool	editRight = true;
    int		editLaneIndex = 0;
    void incrementLane(int index, float _amount, roadSection* _road);
    void shiftLaneSelect(int i) {
        if (editRight) {
            editLaneIndex += i;
            if (editLaneIndex > 8) editLaneIndex = 8;
            if (editLaneIndex < 0) {
                editLaneIndex = 0;
                editRight = false;
                return;
            }
        }
        else {
            editLaneIndex -= i;
            if (editLaneIndex > 8) editLaneIndex = 8;
            if (editLaneIndex < 0) {
                editLaneIndex = 0;
                editRight = true;
                return;
            }
        }

    }
    void setEditRight(bool _r) { editRight = _r; }
    void setEditLane(int _l) { editLaneIndex = _l; }

    static roadMaterialCache roadMatCache;
    void loadRoadMaterial(uint _side, uint _slot);
    void clearRoadMaterial(uint _side, uint _slot);
    uint getRoadMaterial(uint _side, uint _slot, uint _index);
    int selectFrom;
    int selectTo;
    uint selectionType = 0;
    const std::string getMaterialName(uint _side, uint _slot);
    const std::string getMaterialPath(uint _side, uint _slot);
    const Texture::SharedPtr getMaterialTexture(uint _side, uint _slot);
    static Texture::SharedPtr displayThumbnail;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        if (version >= 101) {
            archive(CEREAL_NVP(roadMatCache));
        }
        archive(CEREAL_NVP(roadSectionsList));
        archive(CEREAL_NVP(intersectionList));
    }
};

CEREAL_CLASS_VERSION(roadNetwork, 101);
