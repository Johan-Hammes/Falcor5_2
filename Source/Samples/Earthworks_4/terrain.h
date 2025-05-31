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

#include <thread>
#include "Barrier.hpp"

#include"terrafector.h"
#include "roadNetwork.h"
#include "Sprites.h"
#include "ecotope.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/archives/xml.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"

#include <fstream>

#include "../../external/openJPH/include/openjph/ojph_arg.h"
#include "../../external/openJPH/include/openjph/ojph_mem.h"
#include "../../external/openJPH/include/openjph/ojph_file.h"
#include "../../external/openJPH/include/openjph/ojph_codestream.h"
#include "../../external/openJPH/include/openjph/ojph_params.h"
#include "../../external/openJPH/include/openjph/ojph_message.h"

#include "vegetationBuilder.h"
//#include"hlsl/terrain/vegetation_defines.hlsli"
//#pragma optimize("", off)

#include "glider.h"
#include "cfd.h"
using namespace Falcor;




struct _shadowEdges
{
    float height[4096][4096];
    float Nx[4095][4095];   // temp
    //float3 norm[4095][4095];
    unsigned char edge[4096][4096];

    float2 shadowH[4096][4096];

    void load(std::string filename, float _angle);
    void solve(float _angle, bool dx);

    float sunAngle = 0.02f;  // just afetr sunruise
    float dAngle = 0.01f;
    float3 sunAng;
    bool shadowReady = false;
    bool requestNewShadow = false;
    void solveThread();
};









struct _leaf
{
    void renderGui(Gui* _gui);
    void buildLeaf(float _age, float _lodPixelsize, int _seed, glm::mat4 vertex, glm::vec3 _pos, glm::vec3 _twigAxis, int overrideLod = -1);
    void loadLeafMaterial();
    void load();
    void save();
    void reloadMaterials();
    

    // going to do all this in mm
    float2  stem_length = { 0.f, 0.3f };
    float   stem_width = 4.f;
    float2  stem_curve = { 0.0f, 0.3f };      // radian bend over lenth
    bool    integrate_stem = false;
    bool    cameraFacing = false;
    float2  leaf_length = { 100.f, 0.2f };
    float2  leaf_width = { 60.f, 0.2f };
    float2  stemtoleaf_curve = { 0.0f, 0.2f };      // radian bend over lenth
    float2  leaf_curve = { 0.0f, 0.2f };      // radian bend over lenth
    float2  leaf_twist = { 0.0f, 0.2f };      // radian bend over lenth
    float2  width_offset = { 1.0f, 0.1f };
    float   gravitrophy = 0;

    _vegetationMaterial stem_Material;
    _vegetationMaterial leaf_Material;
    float albedoScale = 1;
    float albedoScaleNew = 1;
    float translucencyScale = 1;
    float translucencyScaleNew = 1;


    int lod = 3;
    int minVerts = 3;
    int maxVerts = 12;

    // FIXME turn exytra curve for gorund etc on ogg, strength
    bool changed = false;
    float maxDistanceFromStem;


    //std::string textureName;
    //uint material;

    ribbonVertex    ribbon[16384];
    uint   ribbonLength;

    // stalk offset  pos
    // leaf start width maize etc

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float2(stem_length);
        archive(CEREAL_NVP(stem_width));
        archive_float2(stem_curve);
        archive(CEREAL_NVP(integrate_stem));
        archive(CEREAL_NVP(cameraFacing));
        
        archive_float2(leaf_length);
        archive_float2(leaf_width);
        archive_float2(stemtoleaf_curve);
        archive_float2(leaf_curve);
        archive_float2(leaf_twist);
        archive_float2(width_offset);
        archive(CEREAL_NVP(gravitrophy));

        archive(CEREAL_NVP(stem_Material));
        archive(CEREAL_NVP(leaf_Material));

        archive(CEREAL_NVP(albedoScale));
        archive(CEREAL_NVP(albedoScaleNew));
        archive(CEREAL_NVP(translucencyScale));
        archive(CEREAL_NVP(translucencyScaleNew));

        if (_version > 100) {
            archive(CEREAL_NVP(minVerts));
            archive(CEREAL_NVP(maxVerts));
        }

        reloadMaterials();
    }
};
#define _LEAF_VERSION 101
CEREAL_CLASS_VERSION(_leaf, _LEAF_VERSION);


#define RIBBON_BLOCK_SIZE 32
struct _twigLod
{
    void load();
    void save();

    bool useSprite;
    int  minSpriteSegments;
    int  maxSpriteSegments;
    //??? smaller inner sprite with leaves

    bool rotateLeaves;
    int leaf_lod;       // ??? just this 0 is rotated
    

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(useSprite));
        archive(CEREAL_NVP(minSpriteSegments));
        archive(CEREAL_NVP(maxSpriteSegments));

        archive(CEREAL_NVP(rotateLeaves));
        archive(CEREAL_NVP(leaf_lod));
    }
};
CEREAL_CLASS_VERSION(_twigLod, 100);





struct _twig
{
    void renderGui(Gui* _gui);
    void build(float _age, float _lodPixelsize, glm::mat4 start, int rndSeed = 100, int overrideLod = -1, float scale = 1.0f);
    void load();
    void save();
    void saveas();
    void loadStemMaterial();
    void reloadMaterials();
    void bakeToMaterial();

    std::string filename = "";
    std::string filepath = "";

    bool    has_stem = true;
    float2  stem_length = float2(50.f, 0.2f);
    float   numSegments = 5.3f;
    int     startSegment = 1;
    float   stem_width = 5.f;
    float2  stem_curve = { 0.2f, 0.3f };      // radian bend over lenth
    float   stem_phototropism = 0.0f;
    bool    tipleaves = false;
    float2  stem_stalk = float2(0.f, 1.f);

    int     numLeaves = 4;  // per segment
    float   stemRotation = 0.7f;   // like 2 leaves 90 degrees
    float   leaf_age_power = 2.f;
    bool    twistAway = false;      // if single leaf, activelt twist to the other side
    _leaf   leaves;


    _vegetationMaterial stem_Material;
    _vegetationMaterial sprite_Material;

    bool changed = false;
    bool changedForSaving = false;
    bool showLeaves;

    ribbonVertex     ribbon[16384];
    uint    ribbonLength;
    float2  extents;        // cylinder

    int lod = 3;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(has_stem));
        archive(CEREAL_NVP(numSegments));
        archive(CEREAL_NVP(startSegment));
        
        archive_float2(stem_length);
        archive(CEREAL_NVP(stem_width));
        archive_float2(stem_curve);
        archive(CEREAL_NVP(stem_phototropism));
        archive(CEREAL_NVP(stem_Material));

        archive(CEREAL_NVP(numLeaves));
        archive(CEREAL_NVP(stemRotation));
        archive(CEREAL_NVP(twistAway));
        archive(CEREAL_NVP(leaves));

        if (_version > 100) {
            archive(CEREAL_NVP(tipleaves));
            archive_float2(stem_stalk);
            archive(CEREAL_NVP(leaf_age_power));
        }

        reloadMaterials();
    }
};
#define _TWIG_VERSION 101
CEREAL_CLASS_VERSION(_twig, _TWIG_VERSION);



struct _twigCollection
{
    void renderGui(Gui* _gui);

    _twig   twig;
    int     numTwigs = 10;
    float   radius = 0.1f;
    float   radiusAngle = 0.5f;
    float   radiusAgeScale = 0.8f;
    float   rnd = 0.2f;

    bool changed = false;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(twig));
        archive(CEREAL_NVP(numTwigs));
        archive(CEREAL_NVP(radius));
        archive(CEREAL_NVP(radiusAngle));
        archive(CEREAL_NVP(radiusAgeScale));
        archive(CEREAL_NVP(rnd));
    }
};
CEREAL_CLASS_VERSION(_twigCollection, 100);


struct _weedLight
{
    float baseheight = 0.5f;
    float width = 0.3f;
    float height = 1.0f;

    float density = 1.0f;
    float cosTop = -0.3f;
    float cosBottom = 0.5f;

    float Ao_depthScale = 0.3f;     // unused
    float sunTilt = -0.2f;
    float bDepth = 10.0f;
    float bScale = 0.5f;

    float4 lightVertex(float3 _pos, float * depth);
    void renderGui(Gui* _gui);

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(baseheight));
        archive(CEREAL_NVP(width));
        archive(CEREAL_NVP(height));
        archive(CEREAL_NVP(density));
        archive(CEREAL_NVP(cosTop));
        archive(CEREAL_NVP(cosBottom));
        archive(CEREAL_NVP(Ao_depthScale));
        archive(CEREAL_NVP(sunTilt));
        archive(CEREAL_NVP(bDepth));
        archive(CEREAL_NVP(bScale));
    }

};
CEREAL_CLASS_VERSION(_weedLight, 100);

struct _weed
{
    void renderGui(Gui* _gui);
    void build(float _age, float _lodPixelsize);
    void load();
    void save();
    void saveas();

    std::string filename = "";
    std::string filepath = "";

    int     rndSeed = 0;
    float   numSegments = 5.3f;
    //_leaf   leaves;
    std::vector<_twigCollection> twigs;

    bool changed = false;
    bool changedForSaving = false;

    ribbonVertex     ribbon[16384]; 
    uint    ribbonLength;
    float2  extents;        // cylinder
    int     lod = 3;
    int visibleTwig = -1;

   _weedLight L;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(rndSeed));
        archive(CEREAL_NVP(numSegments));

        archive(CEREAL_NVP(twigs));
    }
};
#define _WEED_VERSION 100
CEREAL_CLASS_VERSION(_weed, _WEED_VERSION);




struct _leafNode
{
    float3 pos;
    float3 dir;
    int branchNode;
    int branchIndex;

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
    bool isNodeVisible = true;  // dont save this is temporary

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
    bool isVisible = true;
    bool isDead = false;
    

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


struct treeVis
{
    bool expandToLeaves = false;
    bool expandToRoot = false;
    float radiusCutoff = 100;
    float coreRadius = 0.f;
    bool SmallerThan = true;
    float TestSize = 1000;
};

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
    void rebuildRibbons_Leaf();
    void rebuildRibbons_Twig();
    void rebuildRibbons_Weed();
    void findSideBranches();
    void propagateDead(int root);
    int numSideBranchesFound;
    int numDeadEnds;
    int numBadEnds;
    bool showOnlyUnattached = false;

    // Visibility
    void rebuildVisibility();
    treeVis vis;        // expand for lods so we can save it

    

    _weedLight L;
    void calcLight();

    float treeHeight = 0.f;


    ribbonVertex branchRibbons[1024*1024*10];
    float2  extents;        // cylinder
    ribbonVertex8 packedRibbons[1024 * 1024 * 10];
    int numBranchRibbons;
    bool bChanged = false;
    bool includeBranchLeaves = false;
    bool includeEndLeaves = false;

    // optimize, move to a lod calss
    float startRadius = 1000.f;
    float endRadius = 0.005f;
    float stepFactor = 15.0f;
    float bendFactor = 0.95f;

    float getScale() {    return objectSize / 16384.0f;    }
    float3 getOffset() {  return objectOffset * objectSize;  }
    float objectSize = 32.0f;  //0.002 for trees  // 32meter block 2mm presision * 16384 to get actual size in meters
    float radiusScale = 1.0f;//  so biggest radius now objectScale / 2.0f;
    float3 objectOffset = float3(0.5, 0.1f, 0.5f);

    //float Ao_depthScale = 0.3f;
    //float sunTilt = -0.2f;
    //float bDepth = 20.0f;
    //float bScale = 0.5f;

    _twig branchTwigs;
    _twig tipTwigs;
    float tipTwigsAge = 4.5f;
    float branchTwigsAge = 4.5f;
    float twigSkip = 0.f;
    _vegetationMaterial branch_Material;

    _leaf leafBuilder;
    _twig twigBuilder;
    _weed weedBuilder;

    enum _mode {mode_grove, mode_weed, mode_twig, mode_leaf};
    _mode rootMode = mode_twig;

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

        _archive(CEREAL_NVP(branchTwigs));
        _archive(CEREAL_NVP(tipTwigs));
        _archive(CEREAL_NVP(branch_Material));
        _archive(CEREAL_NVP(tipTwigsAge));
        _archive(CEREAL_NVP(branchTwigsAge));
        _archive(CEREAL_NVP(twigSkip));

        if (_version >= 101)
        {
            _archive(CEREAL_NVP(gpuPlant.Ao_depthScale));
            _archive(CEREAL_NVP(gpuPlant.sunTilt));
            _archive(CEREAL_NVP(gpuPlant.bDepth));
            _archive(CEREAL_NVP(gpuPlant.bScale));

            _archive(CEREAL_NVP(objectSize));
            _archive(CEREAL_NVP(radiusScale));
            archive_float3(objectOffset);            
        }

        reloadMaterials();
    }

    void renderGui(Gui* _gui);
    void loadStemMaterial();
    void reloadMaterials();
    void load();

    // GPU data
    plant gpuPlant;
    void toPlant();
};
CEREAL_CLASS_VERSION(_GroveTree, 101);



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
    std::string stamps = "";
    std::string roadMaterial = "X:/resources/terrafectors_and_road_materials/roads/sidewalk_Asphalt.roadMaterial";
    std::string terrafectorMaterial = "";
    std::string texture = "";
    std::string fbx = "";
    std::string EVO = "";

    std::string weed = "F:/terrains/_resources/vegetation_weeds/";
    std::string twig = "F:/terrains/_resources/vegetation_twigs/";
    std::string leaves = "F:/terrains/_resources/vegetation_leaves/";
    std::string trees = "F:/terrains/_resources/vegetation_trees/";
    std::string vegMaterial = "F:/terrains/_resources/vegetation_trees/";


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
        if (_version > 100)
        {
            _archive(CEREAL_NVP(weed));
            _archive(CEREAL_NVP(twig));
            _archive(CEREAL_NVP(leaves));
            _archive(CEREAL_NVP(trees));
            _archive(CEREAL_NVP(vegMaterial));
        }
        if (_version > 101)
        {
            _archive(CEREAL_NVP(stamps));
        }
    }
};
CEREAL_CLASS_VERSION(_lastFile, 102);


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
    uint    imageHash;
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

struct jp2Map {
    void set(uint lod, uint y, uint x, float wSize = 40000.f, float wOffset = -20000.f);
    void save(std::ofstream &_os);
    void saveBinary(std::ofstream &_os);
    void loadBinary(std::ifstream &_is);

    uint lod;
    uint y;
    uint x;

    float2 origin;
    float size;

    float hgt_offset = 0;       // elevation on;y
    float hgt_scale = 0;

    uint fileOffset = 0;
    uint sizeInBytes = 0;


    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(CEREAL_NVP(lod));
        archive(CEREAL_NVP(y));
        archive(CEREAL_NVP(x));

        archive(CEREAL_NVP(origin.x));
        archive(CEREAL_NVP(origin.y));
        archive(CEREAL_NVP(size));

        archive(CEREAL_NVP(hgt_offset));
        archive(CEREAL_NVP(hgt_scale));

        archive(CEREAL_NVP(fileOffset));
        archive(CEREAL_NVP(sizeInBytes));
    }
};

struct jp2File
{
    void save(std::ofstream &_os);
    void saveBinary(std::ofstream &_os);
    void loadBinary(std::ifstream &_is);
    std::string filename;
    std::vector<jp2Map> tiles;
    uint sizeInBytes;
    uint32_t hash;


    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(CEREAL_NVP(filename));
        archive(CEREAL_NVP(hash));
        archive(CEREAL_NVP(sizeInBytes));
        archive(CEREAL_NVP(tiles));
    }
};


struct jp2data {
    std::vector<unsigned char> data;
};

struct jp2Dir
{
    void save(std::string _name);
    void load(std::string _name);
    void cache0(std::string _path);
    void cacheHash(uint32_t hash);
    std::string path;


    void saveBinary(std::string _name);
    void loadBinary(std::string _name);


    std::vector<jp2File> files;

    std::map<uint32_t, uint> fileHashmap;
    std::map<uint32_t, uint2> tileHash;
    LRUCache<uint32_t, std::shared_ptr<std::vector<unsigned char>>> cache;
    //LRUCache<uint32_t, std::vector<unsigned char>> cache;
    std::vector<unsigned char> dataRoot;

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(files);
    }
};

#include "atmosphere.h"

enum _terrainMode { vegetation, ecotope, terrafector, roads, glider };

class terrainManager
{
public:
    terrainManager();
    virtual ~terrainManager();

    void onLoad(RenderContext* _renderContext, FILE* _logfile);
    void onShutdown();
    void onGuiRender(Gui* pGui, fogAtmosphericParams* pAtmosphere);
    void onGuiRenderParaglider(Gui::Window &_window, Gui* pGui, float2 _screen, fogAtmosphericParams *pAtmosphere);
    void onGuiRendercfd(Gui::Window& _window, Gui* pGui, float2 _screen);
    void onGuiRendercfd_params(Gui::Window& _window, Gui* pGui, float2 _screen);
    void onGuiRendercfd_skewT(Gui::Window& _window, Gui* pGui, float2 _screen);
    bool renderGui_Menu = false;
    bool renderGui_Hud = true;
    void onGuiMenubar(Gui* pGui);
    void onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& _fbo, Camera::SharedPtr _camera, GraphicsState::SharedPtr _graphicsState, GraphicsState::Viewport _viewport, Texture::SharedPtr _hdrHalfCopy);
    bool onKeyEvent(const KeyboardEvent& keyEvent);
    bool onMouseEvent(const MouseEvent& mouseEvent, glm::vec2 _screenSize, glm::vec2 _mouseScale, glm::vec2 _mouseOffset, Camera::SharedPtr _camera);
    bool onMouseEvent_Roads(const MouseEvent& mouseEvent, glm::vec2 _screenSize, Camera::SharedPtr _camera);
    bool onMouseEvent_Stamps(const MouseEvent& mouseEvent, glm::vec2 _screenSize, Camera::SharedPtr _camera);
    void onHotReload(HotReloadFlags reloaded);

    void init_TopdownRender();
    void allocateTiles(uint numT);			// ??? FIXME pass shader in as well to allocate GPU memory
    void reset(bool _fullReset = false);
    void loadElevationHash(RenderContext* pRenderContext);
    void loadImageHash(RenderContext* pRenderContext);

    void bil_to_jp2();
    void bil_to_jp2(std::string file, const uint size, FILE* summary, uint _lod, uint _y, uint _x, float _xstart, float _ystart, float _size);

    void generateGdalPhotos();
    void writeGdal(jp2Map _map, std::ofstream &_gdal, std::string _input);
    void writeGdalClear(std::ofstream &_gdal);
    void bil_to_jp2Photos();
    void bil_to_jp2Photos(std::string file, const uint size, uint _lod, uint _y, uint _x);
    uint bil_to_jp2PhotosMemory(std::ofstream& _file, std::string filename, const uint size, uint _lod, uint _y, uint _x);

    void clearCameras();
    void setCamera(unsigned int _index, glm::mat4 viewMatrix, glm::mat4 projMatrix, float3 position, bool b_use, float _resolution);
    bool update(RenderContext* pRenderContext);

    static _lastFile lastfile;

private:
    void testForSurfaceMain();
    void testForSurfaceEnv();
    bool testForSplit(quadtree_tile* _tile);
    bool testFrustum(quadtree_tile* _tile);
    void markChildrenForRemove(quadtree_tile* _tile);

    void hashAndCache(quadtree_tile* pTile);
    void hashAndCacheImages(quadtree_tile* pTile);
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
    void updateDynamicStamp();

    void bezierRoadstoLOD(uint _lod);

    uint                        numTiles = 997;
    std::vector<quadtree_tile>	m_tiles;
    std::list<quadtree_tile*>	m_free;
    std::list<quadtree_tile*>	m_used;
    unsigned int frustumFlags[2048];
public:
    bool fullResetDoNotRender = false;
private:
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
public:
    pixelShader terrainShader;
    pixelShader terrainSpiteShader;
    pixelShader triangleShader;
    _terrainSettings settings;

private:
    
    Texture::SharedPtr	  spriteTexture = nullptr;
    Texture::SharedPtr	  spriteNormalsTexture = nullptr;

    pixelShader vegetationShader;
    pixelShader vegetationShader_Bake;
    Buffer::SharedPtr       ribbonDataVegBuilder;

    pixelShader ribbonShader;
    uint numLoadedRibbons;  // paraglider only
    Buffer::SharedPtr       ribbonData[2];  // also paraglider  - split these into seperate block at least, not true groveTree.bChanged writes to this, so duplicate maybe
    uint bufferidx = 0;
    //std::vector< Texture::SharedPtr> ribbonTextures;        // remove becomes part of the cache
    
    Buffer::SharedPtr       triangleData;
    Fbo::SharedPtr		bakeFbo_plants;
    GraphicsState::Viewport     viewportVegbake;
    //bool bakeOneVeg = false;
    BlendState::SharedPtr           blendstateVegBake;
    //void bakeVegetation(int baseSize = 64, float clipCenter = 1.f, std::string _ext = "_BB");
    void bakeVegetation(int baseSize = 64, float clipCenter = 1.f, std::string _ext = "_billboard");
    computeShader		compute_bakeFloodfill;
    int ribbonInstanceNumber = 1;
    float ribbonSpacing = 3.0f;             // the size fo the extents
    bool spacingFromExtents = true;

    pixelShader thermalsShader;
    Buffer::SharedPtr       thermalsData;
    uint numThermals = 200;

    pixelShader cfdSliceShader;

    public:
    pixelShader         rappersvilleShader;
    Buffer::SharedPtr   rappersvilleData;
    //Buffer::SharedPtr   drawArgs_rappersville;
    int numrapperstri;

    pixelShader         gliderwingShader;
    uint    wingloadedCnt;
    Buffer::SharedPtr   gliderwingData[2];
    RasterizerState::SharedPtr      rasterstateGliderWing;

    bool useFreeCamWhileGliding = false;
    bool GliderDebugVisuals = false;

    
    
    _terrainMode terrainMode = _terrainMode::vegetation;
    private:
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

    jp2Dir imageDirectory;  
    //std::map<uint32_t, heightMap> imageTileHashmap;
    LRUCache<uint32_t, textureCacheElement> imageCache; // move t indese jp2 calss

    terrafectorSystem		terrafectors;
    ecotopeSystem			mEcosystem;
    roadNetwork			    mRoadNetwork;
    splineTest			    splineTest;
    spriteRender			mSpriteRenderer;

    
    stampCollection         mRoadStampCollection;   // all of teh terrafector stamps going over roads
    stamp                   mCurrentStamp;
    int                     stampEditPosisiton = 0;
    void loadStamp()
    {
        std::ifstream is(mRoadStampCollection.lastUsedFilename, std::ios::binary);
        if (!is.fail()) {
            cereal::BinaryInputArchive archive(is);
            archive(mRoadStampCollection);
            

            mRoadStampCollection.reloadMaterials();
            terrafectorEditorMaterial::static_materials.rebuildAll();
            allStamps_to_Terrafector();
        }
    }
    void saveStamp()
    {
        std::ofstream os(mRoadStampCollection.lastUsedFilename, std::ios::binary);
        if (!os.fail()) {
            cereal::BinaryOutputArchive archive(os);
            archive(mRoadStampCollection);
        }
    }
    void stamp_to_Bezier(stamp& S, cubicDouble* BEZ, bezierLayer* IDX, int _index);
    void currentStamp_to_Bezier();
    void allStamps_to_Bezier();
    void allStamps_to_Terrafector();
    

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
        //computeShader		compute_tileElevationMipmap;
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
        uint32_t numDynamicStampIndex = 0;
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
public:
    Sampler::SharedPtr			sampler_Trilinear;
    Sampler::SharedPtr			sampler_Clamp;
    Sampler::SharedPtr			sampler_ClampAnisotropic;
    Sampler::SharedPtr			sampler_Ribbons;


    _shadowEdges shadowEdges;
    Texture::SharedPtr	  terrainShadowTexture;

private:

    bool requestPopupTree = false;
    _GroveTree groveTree;
//public:
    //static materialCache_plants    vegetationMaterialCache;

    public:
    struct
    {
        bool show = false;
        Texture::SharedPtr	  skyTexture;
        Texture::SharedPtr	  envTexture;
        Texture::SharedPtr	  dappledLightTexture;
        // sky mesh

        //_GroveTree groveTree;
        int numSegments = 2000;

        Buffer::SharedPtr blockData;
        Buffer::SharedPtr instanceData;
        Buffer::SharedPtr plantData;
        Buffer::SharedPtr vertexData;
        std::array<plant, 256> plantBuf;
        std::array<plant_instance, 16384> instanceBuf;
        std::array<block_data, 16384> blockBuf;
        std::array<ribbonVertex8, 128 * 256> vertexBuf;



        _rootPlant ROOT;

    }vegetation;
    private:
    bool showGUI = true;

    


    _gliderBuilder paraBuilder;
    _gliderRuntime paraRuntime;
    _new_gliderRuntime newGliderRuntime;
    _airSim AirSim;
    //_cfd CFD;
    //_cfdClipmap cfdClip;
    public:
    

    void cfdStart();
    void cfdThread();
    void paragliderThread(BarrierThrd& bar);
    void paragliderThread_B(BarrierThrd& bar);
    bool requestParaPack = false;
    float3 paraCamPos;
    float3 paraEyeLocal;

    struct
    {
        _cfdClipmap clipmap;
        std::string rootPath;
        std::string rootFile;
        bool recordingCFD = false;
        int cfd_play_k = 0;

        // instead fo mutex, I am going to do oneay coms for now
        // input
        float stepTime;
        bool realTime = true;   // if false we run as fast as posible to solve as much time
        uint exportFrameStep = 0;   // 0 does not export, typical is repeating frame 2^lod - 1 every time we have sync

        std::array<float3, 10> velocityRequets;
        float3 originRequest = float3(-1425 - 0, 425 + 140, 14533 - 2000);
        bool pause = false;


        // output
        bool newFlowLines = false;
        uint numLines = 0;
        std::vector<float4> flowlines;

        std::array<float3, 10> velocityAnswers;

        // skewT editor ---------------------------------
        std::array<float3, 100> skewT_data;
        std::array<float3, 100> skewT_V;
        bool editMode = false;
        bool editChanged = false;


        Texture::SharedPtr	  sliceVTexture[2];
        Texture::SharedPtr	  sliceDataTexture[2];
        Texture::SharedPtr	  sliceVolumeTexture[6][3];
        //uint sliceOrder[6] = { 0, 0, 0, 0, 0, 0 };
        float sliceTime[6] = { 0, 0, 0, 0, 0, 0 };
        float lodLerp[6] = { 0, 0, 0, 0, 0, 0 };
    } cfd;      // guess this needs to become its own class or move into _cfdClipmap


    struct
    {
        bool newGliderLoaded = false;

        bool loaded = false;
        float frameTime;
        float packTime;

        float cellsTime[2];
        float preTime[2];
        float wingTime[2];
        float postTime[2];
    } glider;
};
