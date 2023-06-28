/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once
#include "Falcor.h"
#include "computeShader.h"
#include "pixelShader.h"
//#include"hlsl/groundcover_defines.hlsli"
//#include"hlsl/terrainDefines.hlsli"
//#include"hlsl/gpuLights_defines.hlsli"
#include"terrafector.h"
#include "roadNetwork.h"
#include "Sprites.h"
#include "ecotope.h"

#include "../../external/cereal-master/include/cereal/cereal.hpp"
#include "../../external/cereal-master/include/cereal/archives/binary.hpp"
#include "../../external/cereal-master/include/cereal/archives/json.hpp"
#include "../../external/cereal-master/include/cereal/archives/xml.hpp"
#include <fstream>
//#define archive_float2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
//#define archive_float3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
//#define archive_float4(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z)); archive(CEREAL_NVP(v.w));}


#include "../../external/openJPH/include/openjph/ojph_arg.h"
#include "../../external/openJPH/include/openjph/ojph_mem.h"
#include "../../external/openJPH/include/openjph/ojph_file.h"
#include "../../external/openJPH/include/openjph/ojph_codestream.h"
#include "../../external/openJPH/include/openjph/ojph_params.h"
#include "../../external/openJPH/include/openjph/ojph_message.h"

using namespace Falcor;



struct _leafNode
{
    float3 pos;
    float3 dir;
    int branchNode;

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(pos));
        _archive(CEREAL_NVP(dir));
        _archive(CEREAL_NVP(branchNode));
    }
};
CEREAL_CLASS_VERSION(_leafNode, 100);



struct _cubemap
{
    // this is about every 5 drgrees
#define cubeHalfSize 32
    struct _data
    {
        float d;
        float sum;      // for averaging into buckets esencially
        float3 dir;
        float cone;     // cosTheta value
    };

    float3 center;
    float3 scale;
    float twigOffset = 0.2f;

    int face, y, x;
    float dy, dx;
    void toCube(float3 _v);
    float3 toVec(int face, int y, int x);
    float sampleDistance(float3 _v);
    void clear();
    void writeDistance(float3 _v);
    void solveEdges();
    void solve();

    float4 light(float3 p, float* _depth);

    _data data[6][cubeHalfSize*2 + 2][cubeHalfSize*2 + 2];
};

struct _branchnode
{
    float3 pos;
    float radius;
    float3 dir;

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(pos));
        _archive(CEREAL_NVP(radius));
        _archive(CEREAL_NVP(dir));
    }
};
CEREAL_CLASS_VERSION(_branchnode, 100);

struct _GroveBranch
{
    
    std::vector<_branchnode> nodes;
    std::vector <_leafNode> leaves;
    std::vector<int> sideBranches;
    int rootBranch = -1;
    int sideNode = 0;
    

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(nodes));
        _archive(CEREAL_NVP(leaves));
        _archive(CEREAL_NVP(sideBranches));
        _archive(CEREAL_NVP(sideBranch));
        _archive(CEREAL_NVP(sideNode));
    }

    void reset();
};
CEREAL_CLASS_VERSION(_GroveBranch, 100);



struct _GroveTree
{
    std::string filename;
    int numFour = 3;       // number of start segments with 4 verts, try to auto detect
    float scale = 100.0f;
    std::vector<_GroveBranch> branches;
    std::vector<_leafNode> endLeaves;
    std::vector<_leafNode> branchLeaves;
    
    // lodding stuff
    FILE* objfile;
    float3 verts[100];
    int numVerts;
    int oldNumVerts;
    bool enfOfFile;
    bool branchMode;
    bool isPlanar;
    int totalVerts;
    float3 nodeDir;
    float nodeOffset;
    _GroveBranch* currentBranch;
    void readHeader();
    void read2();
    void readahead1();
    float3 readVertex();
    void testBranchLeaves();
    void rebuildRibbons();
    void findSideBranches();
    int numSideBranchesFound;
    int numDeadEnds;
    int numBadEnds;
    bool showOnlyUnattached = false;

    void calcLight();


    ribbonVertex branchRibbons[1024*1024*10];
    int numBranchRibbons;
    bool bChanged = false;
    bool includeBranchLeaves = false;
    bool includeEndLeaves = false;

    // optimize, move to a lod calss
    float startRadius = 1000.f;
    float endRadius = 0.f;
    float stepFactor = 2.0f;
    float bendFactor = 0.95f;


    // twigs
    ribbonVertex twig[1024];
    uint twiglength = 0;
    float twigScale = 1.0f;
    void importTwig();

    struct {
        glm::vec3 center;
        glm::vec3 Min;
        glm::vec3 Max;
        glm::vec3 scale;
        //float distCube[6][16][16];
        _cubemap cubemap;
    } light;

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(filename));
        _archive(CEREAL_NVP(numFour));
        _archive(CEREAL_NVP(scale));
        _archive(CEREAL_NVP(branches));
    }

    void renderGui(Gui* _gui);
    void load();
};
CEREAL_CLASS_VERSION(_GroveTree, 100);



template<typename K, typename V = K>
class LRUCache
{

private:
    std::list<K>items;
    std::unordered_map <K, std::pair<V, typename std::list<K>::iterator>> keyValuesMap;
    uint csize = 50;	// arbitrary default

public:
    LRUCache() { ; }

    void resize(uint s) {
        csize = s;
        items.clear();
        keyValuesMap.clear();
    }

    void set(const K key, const V value) {
        auto pos = keyValuesMap.find(key);
        if (pos == keyValuesMap.end()) {
            items.push_front(key);
            keyValuesMap[key] = { value, items.begin() };
            if (keyValuesMap.size() > csize) {
                keyValuesMap.erase(items.back());
                items.pop_back();
            }
        }
        else {
            items.erase(pos->second.second);
            items.push_front(key);
            keyValuesMap[key] = { value, items.begin() };
        }
    }

    bool get(const K key, V& value) {
        auto pos = keyValuesMap.find(key);
        if (pos == keyValuesMap.end())
            return false;
        items.erase(pos->second.second);
        items.push_front(key);
        keyValuesMap[key] = { pos->second.first, items.begin() };
        value = pos->second.first;
        return true;
    }
};


struct _lastFile
{
    // these are for quick load
    //std::string terrain = "X:/resources/terrains/eifel/eifel.terrain";
    std::string terrain = "F:/terrains/sonoma/sonoma.terrain";
    std::string road = "X:/resources/terrains/eifel/roads/day6.roadnetwork";
    std::string roadMaterial = "X:/resources/terrafectors_and_road_materials/roads/sidewalk_Asphalt.roadMaterial";
    std::string terrafectorMaterial = "";
    std::string texture = "";
    std::string fbx = "";
    std::string EVO = "C:/Kunos/acevo_content/content";


    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(terrain));
        _archive(CEREAL_NVP(road));
        _archive(CEREAL_NVP(roadMaterial));
        _archive(CEREAL_NVP(terrafectorMaterial));
        _archive(CEREAL_NVP(texture));
        _archive(CEREAL_NVP(fbx));
        _archive(CEREAL_NVP(EVO));
    }
};
CEREAL_CLASS_VERSION(_lastFile, 100);


struct _terrainSettings
{
    // these are for quick load
    std::string name = "eifel";
    std::string projection = "\" + proj = tmerc + lat_0 = 50.39 + lon_0 = 6.91 + k_0 = 1 + x_0 = 0 + y_0 = 0 + ellps = GRS80 + units = m\"";
    float size = 40000.f;

    std::string dirRoot = "X:/resources/terrains/eifel";
    std::string dirExport = "/terrains/Eifel";
    std::string dirGis = "X:/resources/terrains/eifel";
    std::string dirResource = "X:/resources";

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(name));
        _archive(CEREAL_NVP(projection));
        _archive(CEREAL_NVP(size));
        _archive(CEREAL_NVP(dirRoot));
        _archive(CEREAL_NVP(dirExport));
        _archive(CEREAL_NVP(dirGis));
        _archive(CEREAL_NVP(dirResource));
    }

    void renderGui(Gui* _gui);
};
CEREAL_CLASS_VERSION(_terrainSettings, 100);



// FOR binary export of tiles
struct elevationMap {
    float heightOffset;
    float heightScale;
    float2 origin;
    float tileSize;
    uint lod;
    uint y;
    uint x;
};



class quadtree_tile
{
public:
    void init(uint _index);
    void set(uint _lod, uint _x, uint _y, float _size, float4 _origin, quadtree_tile* _parent);
    void reset() {
        forSplit = false;
        forRemove = false;
    }

private:
public:
    uint index;
    quadtree_tile* parent;
    quadtree_tile* child[4];

    float4 boundingSphere;		// position has to be power of two to allow us to store large world offsets using float rather than double
    float4 origin;
    float   size;
    uint    lod;
    uint    y;
    uint    x;

    bool    	forSplit = false;
    bool	    forRemove = false;          // the children
    bool    	main_ShouldSplit = false;
    bool	    env_ShouldSplit = false;

    uint    numQuads = 0;
    uint    numPlants = 0;
    uint    elevationHash;
};


enum CameraType {
    CameraType_Main_Left,
    CameraType_Main_Center,
    CameraType_Main_Right,
    CameraType_Rear_Left,
    CameraType_Rear_Center,
    CameraType_Rear_Right,
    CameraType_MAX,
};

struct terrainCamera {
    bool bUse;
    float3 position;
    glm::mat4x4 view;
    glm::mat4x4 proj;
    glm::mat4x4 viewProj;
    glm::mat4x4 viewProjTranspose;
    float resolution;
    std::array<float4, 4> frustumPlane;
    glm::mat4x4 frustumMatrix;
};

struct heightMap {
    float2 origin;
    float size;
    float hgt_offset;
    float hgt_scale;
    std::string filename;	// jpeg2000 file

    // for export
    uint lod;
    uint y;
    uint x;
};

class terrainManager
{
public:
    terrainManager();
    virtual ~terrainManager();

    void onLoad(RenderContext* _renderContext, FILE* _logfile);
    void onShutdown();
    void onGuiRender(Gui* pGui);
    void onGuiMenubar(Gui* pGui);
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& _fbo, const Camera::SharedPtr _camera, GraphicsState::SharedPtr _graphicsState, GraphicsState::Viewport _viewport);
    bool onKeyEvent(const KeyboardEvent& keyEvent);
    bool onMouseEvent(const MouseEvent& mouseEvent, glm::vec2 _screenSize, glm::vec2 _mouseScale, glm::vec2 _mouseOffset, Camera::SharedPtr _camera);
    bool onMouseEvent_Roads(const MouseEvent& mouseEvent, glm::vec2 _screenSize, Camera::SharedPtr _camera);
    void onHotReload(HotReloadFlags reloaded);

    void init_TopdownRender();
    void allocateTiles(uint numT);			// ??? FIXME pass shader in as well to allocate GPU memory
    void reset(bool _fullReset = false);
    void loadElevationHash(RenderContext* pRenderContext);

    void bil_to_jp2();
    void bil_to_jp2(std::string file, const uint size, FILE* summary, uint _lod, uint _y, uint _x, float _xstart, float _ystart, float _size);

    void clearCameras();
    void setCamera(unsigned int _index, glm::mat4 viewMatrix, glm::mat4 projMatrix, float3 position, bool b_use, float _resolution);
    bool update(RenderContext* pRenderContext);

private:
    void testForSurfaceMain();
    void testForSurfaceEnv();
    bool testForSplit(quadtree_tile* _tile);
    bool testFrustum(quadtree_tile* _tile);
    void markChildrenForRemove(quadtree_tile* _tile);

    void hashAndCache(quadtree_tile* pTile);
    void setChild(quadtree_tile* pTile, int y, int x);
    void splitOne(RenderContext* _renderContext);
    void splitChild(quadtree_tile* _pTile, RenderContext* _renderContext);
    void splitRenderTopdown(quadtree_tile* _pTile, RenderContext* _renderContext);

    uint gisHash(glm::vec3 _position);
    void gisReload(glm::vec3 _position);

    void bake_start(bool _toMAX);
    bool bakeToMax;
    void bake_frame();
    void bake_Setup(float _size, uint lod, uint y, uint x, RenderContext* _renderContext);
    void bake_RenderTopdown(float _size, uint lod, uint y, uint x, RenderContext* _renderContext);
    void sceneToMax();
    int exportLodMin = 10;
    int exportLodMax = 10;

    void updateDynamicRoad(bool _bezierChanged);

    void bezierRoadstoLOD(uint _lod);

    uint                        numTiles = 997;
    std::vector<quadtree_tile>	m_tiles;
    std::list<quadtree_tile*>	m_free;
    std::list<quadtree_tile*>	m_used;
    unsigned int frustumFlags[2048];
    bool fullResetDoNotRender = false;
#ifdef COMPUTE_DEBUG_OUTPUT
    bool debug = true;
#else
    bool debug = false;
#endif

    std::array<terrainCamera, CameraType_MAX> cameraViews;
    float3 cameraOrigin;

    Texture::SharedPtr compressed_Normals_Array;
    Texture::SharedPtr compressed_Albedo_Array;
    Texture::SharedPtr compressed_PBR_Array;
    Texture::SharedPtr height_Array;

    pixelShader terrainShader;
    pixelShader terrainSpiteShader;
    Texture::SharedPtr	  spriteTexture = nullptr;

    pixelShader ribbonShader;
    Buffer::SharedPtr       ribbonMaterials;
    Buffer::SharedPtr       ribbonData;
    std::vector< Texture::SharedPtr> ribbonTextures;
    pixelShader triangleShader;
    Buffer::SharedPtr       triangleData;

    _lastFile lastfile;
    _terrainSettings settings;
    int terrainMode = 3;
    bool hasChanged = false;

    bool requestPopupSettings = false;
    bool requestPopupDebug = false;

    glm::vec3 mouseDirection;
    glm::vec3 mousePosition;
    glm::vec2 mousePositionOld;
    glm::vec2 screenSize;
    glm::vec2 mouseCoord;
    float mouseVegOrbit = 10;
    float mouseVegPitch = 0.1f;
    float mouseVegYaw = 0.0f;
    float mouseVegRoll = 0.0f;
    computeShader compute_TerrainUnderMouse;

    std::map<uint32_t, heightMap> elevationTileHashmap;
    struct textureCacheElement {
        Texture::SharedPtr	  texture = nullptr;
    };
    LRUCache<uint32_t, textureCacheElement> elevationCache;

    terrafectorSystem		terrafectors;
    ecotopeSystem			mEcosystem;
    roadNetwork			    mRoadNetwork;
    splineTest			splineTest;
    spriteRender				mSpriteRenderer;

    bool bSplineAsTerrafector = false;
    bool showRoadOverlay = true;
    bool showRoadSpline = true;

    bool bLeftButton = false;
    bool bMiddelButton = false;
    bool bRightButton = false;

    struct {
        Texture::SharedPtr texture = nullptr;
        uint hash = 0;
        glm::vec4 box;
        bool show = true;
        float strenght = 0.3f;

        float redStrength = 0.0f;
        float redScale = 5.0f;
        float redOffset = 0.05f;

        float terrafectorOverlayStrength = 0.1f;
        float splineOverlayStrength = 1.f;
        bool bakeBakeOnlyData = true;
    }gis_overlay;

    struct {
        uint                bakeSize = 1024;
        Fbo::SharedPtr		tileFbo;
        Fbo::SharedPtr		bakeFbo;

        Texture::SharedPtr	noise_u16;

        computeShader		compute_tileClear;
        computeShader		compute_tileSplitMerge;
        computeShader		compute_tileGenerate;
        computeShader		compute_tilePassthrough;
        computeShader		compute_tileBuildLookup;
        computeShader		compute_tileBicubic;		// upsample heights with bicubic filter
        computeShader		compute_tileEcotopes;
        computeShader		compute_tileNormals;
        computeShader		compute_tileVerticis;
        computeShader		compute_tileJumpFlood;
        computeShader		compute_tileDelaunay;
        computeShader		compute_tileElevationMipmap;
        computeShader		compute_clipLodAnimatePlants;

        // BC6H compressor
        computeShader           compute_bc6h;
        Texture::SharedPtr      bc6h_texture;

        Buffer::SharedPtr       dispatchArgs_plants;
        Buffer::SharedPtr       drawArgs_quads;
        Buffer::SharedPtr       drawArgs_plants;
        Buffer::SharedPtr       drawArgs_clippedloddedplants;
        Buffer::SharedPtr       drawArgs_tiles;         // block based
        Buffer::SharedPtr       buffer_feedback;
        Buffer::SharedPtr		buffer_feedback_read;
        GC_feedback             feedback;

        std::vector<gpuTile>    cpuTiles;
        Buffer::SharedPtr       buffer_tiles;
        Buffer::SharedPtr       buffer_tiles_readback;
        Buffer::SharedPtr       buffer_instance_quads;
        Buffer::SharedPtr       buffer_instance_plants;
        Buffer::SharedPtr       buffer_clippedloddedplants;

        Buffer::SharedPtr       buffer_lookup_terrain;
        Buffer::SharedPtr       buffer_lookup_quads;
        Buffer::SharedPtr       buffer_lookup_plants;

        Buffer::SharedPtr	    buffer_tileCenters;
        Buffer::SharedPtr		buffer_tileCenter_readback;
        std::array<float4, 2048>		tileCenters;

        Buffer::SharedPtr       buffer_terrain;

        Texture::SharedPtr      debug_texture;
        //Texture::SharedPtr      bicubic_upsample_texture;
        Texture::SharedPtr      normals_texture;
        Texture::SharedPtr      vertex_A_texture;
        Texture::SharedPtr      vertex_B_texture;
        Texture::SharedPtr      vertex_clear;		                // 0 for fast clear
        Texture::SharedPtr      vertex_preload;	                    // a pre allocated 1/8 verts

        Texture::SharedPtr	    rootElevation;

        pixelShader             shader_spline3D;
        pixelShader             shader_splineTerrafector;
        pixelShader             shader_meshTerrafector;             // these two should merge
        DepthStencilState::SharedPtr    depthstateCloser;
        DepthStencilState::SharedPtr    depthstateFuther;
        DepthStencilState::SharedPtr    depthstateAll;
        RasterizerState::SharedPtr      rasterstateSplines;
        BlendState::SharedPtr           blendstateSplines;
        BlendState::SharedPtr		    blendstateRoadsCombined;
    } split;

    struct
    {
        uint32_t maxBezier = 131072;            // 17 bits packed - likely to change soon
        uint32_t maxIndex = 524288;             // *4 seems enough, 2022 at *1.7 for Nurburg
        uint32_t numStaticSplines = 0;
        uint32_t numStaticSplinesIndex = 0;
        uint32_t numStaticSplinesBakeOnlyIndex = 0;
        Buffer::SharedPtr bezierData;
        Buffer::SharedPtr indexData;
        Buffer::SharedPtr indexDataBakeOnly;
        Buffer::SharedPtr indexData_LOD4;
        Buffer::SharedPtr indexData_LOD6;
        Buffer::SharedPtr indexData_LOD8;
        uint startOffset_LOD4[16][16];
        uint numIndex_LOD4[16][16];
        uint startOffset_LOD6[64][64];
        uint numIndex_LOD6[64][64];
        uint startOffset_LOD8[256][256];
        uint numIndex_LOD8[256][256];
        std::vector<bezierLayer> lod4[16][16];
        std::vector<bezierLayer> lod6[64][64];
        std::vector<bezierLayer> lod8[256][256];

        uint32_t maxDynamicBezier = 4096;            // 17 bits packed - likely to change soon
        uint32_t maxDynamicIndex = 16384;             // *4 seems enough, 2022 at *1.7 for Nurburg
        uint32_t numDynamicSplines = 0;
        uint32_t numDynamicSplinesIndex = 0;
        Buffer::SharedPtr dynamic_bezierData;
        Buffer::SharedPtr dynamic_indexData;
        uint numIndex = 0;
    }splines;

    struct
    {
        std::vector<uint32_t> tileHash;
        float quality = 0.0002f;
        int roadMaxSplits = 16;
        FILE* txt_file = nullptr;
        bool inProgress = false;
        std::vector<elevationMap> tileInfoForBinaryExport;
        std::vector<uint32_t>::iterator itterator;
        RenderContext* renderContext;
        Texture::SharedPtr	  copy_texture = nullptr;
    }bake;

    struct
    {
        bool        hit;
        glm::vec3   terrain;
        float       panDistance;
        float       cameraHeight;
        glm::vec3   toGround;
        glm::vec3   pan;
        glm::vec3   orbit;
        float       orbitRadius;
        float       mouseToHeightRatio;

        glm::vec3   newPosition;
        glm::vec3   newTarget;
    }mouse;

    Sampler::SharedPtr			sampler_Trilinear;
    Sampler::SharedPtr			sampler_Clamp;
    Sampler::SharedPtr			sampler_ClampAnisotropic;
    Sampler::SharedPtr			sampler_Ribbons;


    bool requestPopupTree = false;
    _GroveTree groveTree;


    struct
    {
        bool show = false;
        Texture::SharedPtr	  skyTexture;
        Texture::SharedPtr	  envTexture;
        // sky mesh

        _GroveTree groveTree;
        int numSegments = 2000;

    }vegetation;
    
};
