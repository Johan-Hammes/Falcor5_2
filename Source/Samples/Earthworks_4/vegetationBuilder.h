#pragma once
#include "Falcor.h"
#include "computeShader.h"
#include "pixelShader.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"

//#include <fstream>

#include <random>

#include"terrafector.h"
#include"hlsl/terrain/vegetation_defines.hlsli"
#include"hlsl/terrain/groundcover_defines.hlsli"    // FIXME combine these two


using namespace Falcor;





class _plantMaterial;

class materialCache_plants {
public:
    static std::string lastFile;   // fixme, try to copy this to and from vegMaterial on start and exit to remember

    int find_insert_material(const std::string _path, const std::string _name, bool _forceReload = false);
    int find_insert_material(const std::filesystem::path _path, bool _forceReload = false);
    int find_insert_texture(const std::filesystem::path _path, bool isSRGB, bool _forceReload = false);
    std::string clean(const std::string _s);

    void setTextures(ShaderVar& var);

    void rebuildStructuredBuffer();
    
    std::vector<_plantMaterial>	materialVector;
    int selectedMaterial = -1;
    std::vector<Texture::SharedPtr>			textureVector;
    float texMb = 0;

    void renderGui(Gui* mpGui, Gui::Window& _window);
    void renderGuiTextures(Gui* mpGui, Gui::Window& _window);
    bool renderGuiSelect(Gui* mpGui);
    
public:
    int dispTexIndex = -1;
    Texture::SharedPtr getDisplayTexture();

    Buffer::SharedPtr sb_vegetation_Materials = nullptr;

    bool modified = false;
    bool modifiedData = false;
};



class _plantMaterial
{
public:
    void renderGui(Gui* _gui);

    static materialCache_plants static_materials_veg;

    std::filesystem::path 			  fullPath;     // FIXME remove ebentuall
    std::string        relativePath;
    void makeRelative(std::filesystem::path _path); // FIXME move to terrafector where we keep the path

    std::string			  displayName = "Not set!";
    bool				isModified = false;

    std::string         albedoName;
    std::string         normalName;
    std::string         translucencyName;
    std::string         alphaName;

    std::string         albedoPath;
    std::string         normalPath;
    std::string         translucencyPath;
    std::string         alphaPath;

    bool changedForSave = false;

    void import(std::filesystem::path _path, bool _replacePath = true);
    void import(bool _replacePath = true);
    void save();
    void eXport(std::filesystem::path _path);
    void eXport();
    void reloadTextures();
    void loadTexture(int idx);

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(albedoName));
        archive(CEREAL_NVP(alphaName));
        archive(CEREAL_NVP(normalName));
        archive(CEREAL_NVP(translucencyName));

        archive(CEREAL_NVP(albedoPath));
        archive(CEREAL_NVP(alphaPath));
        archive(CEREAL_NVP(normalPath));
        archive(CEREAL_NVP(translucencyPath));

        archive(CEREAL_NVP(_constData.translucency));
        archive(CEREAL_NVP(_constData.alphaPow));

        if (_version >= 101)
        {
            archive_float3(_constData.albedoScale[0]);
            archive_float3(_constData.albedoScale[1]);
            archive(CEREAL_NVP(_constData.roughness[0]));
            archive(CEREAL_NVP(_constData.roughness[1]));
        }
    }


    sprite_material _constData;
};
CEREAL_CLASS_VERSION(_plantMaterial, 101);




struct _vegetationMaterial {
    std::string name;
    std::string displayname;
    int index = -1;

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(CEREAL_NVP(name));
        archive(CEREAL_NVP(displayname));
    }
};









struct buildSetting
{
    float3      parentStemDir = { 0, 1, 0 };
    glm::mat4   root; // expand for the root stem direction as well to avoid growing through
    float       numSegments;
    float       pixelSize = 0.001f;  //1 cm
    int         seed = 1000;
    float       node_age = -1.f; // dont use for root    // this one is a node age, like 12, good for building twigs
    float       normalized_age = 1.f;  // this one is a 0..1 age useful for leaves etc
    bool        forcePhototropy = false;    // for billboard baking

    // new values for more complex builds

};

struct bakeSettings
{
    float objectSize = 15.0f;
    float radiusScale = 1.0f;                   //  so biggest radius
    float3 objectOffset = float3(0.5, 0.1f, 0.5f);
    float getScale() { return objectSize / 16384.0f; }
    float3 getOffset() { return objectOffset * objectSize; }
};


//#pragma optimize("", off)

struct ribbonVertex
{
    static float objectScale;   //0.002 for trees  // 32meter block 2mm presision
    static float radiusScale;   //  so biggest radius now objectScale / 2.0f;
    static float O;
    static float3 objectOffset;

    static std::vector<ribbonVertex>    ribbons;
    static std::vector<ribbonVertex8>    packed;
    static bool pushStart;

    static void setup(float scale, float radius, float3 offset);
    ribbonVertex8 pack();

    void    startRibbon(bool _cameraFacing)
    {
        pushStart = false;   // prepare for a new ribbon to start
        type = !_cameraFacing;
        S_root = 0;
    }

    void set(glm::mat4 _node, float _radius, int _material, float2 _uv, float _albedo, float _translucency, bool _clearLeafRoot = true,
        float _stiff = 0.5f, float _freq = 0.1f, float _index = 0.f )
    {

        position = _node[3];
        if (fabs(position.x) > 2 || fabs(position.z) > 2)
        {
            bool bCM = true;
        }
        radius = _radius;
        bitangent = _node[1];  // right handed matrix : up
        tangent = _node[0];
        material = _material;
        uv = _uv;
        albedoScale = _albedo;
        translucencyScale = _translucency;

        leafStiffness = _stiff;
        leafFrequency = _freq;
        leafIndex = _index;

        
        leafRoot = S_root;
        if (_clearLeafRoot)
        {
            S_root = 0;
        }
        else
        {
            S_root++;
        }

        startBit = pushStart;    // badly named its teh inverse, but after the first bit we clear iyt for teh rest of teh ribbon

        uint idx = ribbonVertex::ribbons.size();
        if ((idx > 0) && (idx % VEG_BLOCK_SIZE == 0) && startBit == true)
        {
            // start of a new block, but we are in teh middle of a ribbon, repeat the last one as a start
            ribbonVertex R = ribbonVertex::ribbons.back();
            R.startBit = false;
            
            if (_clearLeafRoot)
            {
                S_root = 0;
            }
            else
            {
                R.leafRoot++; // This is the previous one plus 1 more
                leafRoot = R.leafRoot + 1;  // advance teh new vertex one more
                S_root = leafRoot + 1;
            }

            ribbonVertex::ribbons.push_back(R);
        }

        ribbonVertex::ribbons.push_back(*this);

        pushStart = true;
    }

    float3 egg(float2 extents, float3 vector, float yOffset)
    {
        float3 V = glm::normalize(vector) * extents.x;
        if (V.y > 0)
        {
            V.y *= extents.y * (1.f - yOffset);
        }
        else
        {
            V.y *= extents.y * yOffset;
        }
        return V;
    }

    void lightBasic(float2 extents, float plantDepth, float yOffset)
    {
        float midHeight = yOffset * extents.y;
        float3 Ldir = position - float3(0, midHeight, 0);
        float3 edge = egg(extents, Ldir, yOffset);
        lightCone = float4(glm::normalize(Ldir), 0);    // 0 is just 180 degrees so wide, fixme tighter at ythe bottom
        float depthMeters = __max(0, glm::length(edge) - glm::length(Ldir));
        lightDepth = depthMeters / plantDepth;
        if (position.y < midHeight)
        {
            lightDepth = (depthMeters + (midHeight - position.y)) / plantDepth;
        }

        float3 P = position;
        P.y = 0;
        float dx = glm::length(P);
        float aoW = 0.3f + 0.7f * dx / extents.x;
        float aoH = 0.4f + 0.6f * position.y / extents.y;
        ambientOcclusion = __max(aoW, aoH);
        if (position.y < midHeight)
        {
            float scale = (midHeight - position.y) / midHeight;
            ambientOcclusion *= (1.f - scale * 0.5f);
        }
    }

    // FIXME void light(lightCone light Depth ao shadow)

    int     type = 0;
    bool    startBit = false;
    float3  position;
    int     material;
    float   anim_blend;
    float2  uv;
    float3  bitangent;
    float3  tangent;
    float   radius;

    float4  lightCone = {0, 1, 0, 1};
    float   lightDepth = 0.2f;
    float ambientOcclusion = 1.f;
    unsigned char shadow = 255;         // ??? I think this in now unused
    float albedoScale = 1.f;
    float translucencyScale = 1.f;

    uint root[4] = { 0, 0, 0, 0 };

    static uint S_root;
    uint leafRoot = 0;
    float leafStiffness = 1.f;
    float leafFrequency = 10.f;
    float leafIndex = 0.f;
};





class _vegMaterial
{
public:
    std::string name;
    std::string path;   // relative
    int index = -1;

    float2 albedoScale = { 1, 1 };
    float2 translucencyScale = { 1, 1 };

    void loadFromFile();
    void reload();
    bool renderGui(uint &gui_id);

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(name);
        archive(path);
        archive_float2(albedoScale);
        archive_float2(translucencyScale);
    }
};




class levelOfDetail
{
public:
    levelOfDetail() { ; }
    levelOfDetail(uint _numPix) { numPixels = _numPix; }

    int numPixels = 100;   // this is the number of height pixels to use for this lod. Used to calculate pixel size
    float pixelSize = 0.f;  //auto calculated

    // feedback
    uint numVerts = 0;
    uint numBlocks = 0;
    uint unused = 0;
    uint startBlock = 0;
    

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(CEREAL_NVP(numPixels));
        archive(CEREAL_NVP(pixelSize));
    }
};



class lodBake
{
public:
    lodBake(uint _h, float _w) { pixHeight = _h; bakeWidth = _w; }
    _vegMaterial material;
    float2 extents = {0, 0};
    int pixHeight = 64;
    float bakeWidth = 1.f;      // really a percentage
    std::array<float, 4> dU = {1, 1, 1, 1};

    //bool renderGui(uint& gui_id);

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(material);
        archive_float2(extents);
        archive(pixHeight);
        archive(bakeWidth);
        archive(dU);
        
    }
};


class _plantBuilder
{
public:
    virtual void loadPath() { ; }
    virtual void savePath() { ; }
    virtual void load() { ; }
    virtual void save() { ; }
    virtual void saveas() { ; }
    virtual void renderGui() { ; }
    virtual void treeView() { ; }
    virtual glm::mat4 build(buildSetting _settings, bool _addVerts) { return glm::mat4(1.f); }
    virtual lodBake* getBakeInfo(uint i) { return nullptr; }    // so does nothing if not implimented
    virtual levelOfDetail* getLodInfo(uint i) { return nullptr; }    // so does nothing if not implimented

    std::string name = "not set";
    std::string path = "no path either";   // relative
    bool changed = true;
    bool changedForSave = false;
    static Gui* _gui;

    // lighting information
    float shadowUVScale = 1.f;
    float shadowSoftness = 0.15f;
    float shadowDepth = 1.f;
    float shadowPenetationHeight = 0.3f;

    // ossilations
    float ossilation_stiffness = 1.f;   // this affects how far it bends for certain wind types, but also
    float ossilation_constant_sqrt = 10.f;     //freq = (1 / 2π) * √(g / L). so this is g/L per meter, we scale by a furher √(1/L)
    float ossilation_power = 1.f;       // shifts the bend towards the root or the tip
    float rootFrequency() { return 0.1591549430f * ossilation_constant_sqrt * sqrt(1.f / ossilation_stiffness); }
    int     deepest_pivot_pack_level = 3;
};

enum plantType {P_LEAF, P_STEM, PLANT_END};

// planmaterial repackadged for dandom_arrays
class _plantRND
{
public:
    std::string name;
    std::string path;   // relative
    plantType  type = PLANT_END;
    std::shared_ptr<_plantBuilder> plantPtr;

    void loadFromFile();
    void reload();
    void renderGui(uint& gui_id);

    template<class Archive>
    void serialize(Archive& archive)
    {
/*        if (plantPtr)
        {
            archive(plantPtr->name);
            archive(plantPtr->path);
            archive(type);
        }
        else
        {*/
            archive(name);
            archive(path);
            archive(type);
        //}
    }
};



// would be nice if we have more controll over distribution including with numSegments
template <class T> class randomVector 
{
public:
    randomVector() { data.resize(1); }      // minimum size has to be one, otherwise get() is dangerous
    std::vector<T> data;
    
    void renderGui(char *name, uint& gui_id);
    void clear() { data.clear(); }
    T get();
};





class _leafBuilder : public _plantBuilder
{
public:
    void loadPath();
    void savePath();
    void load();
    void save();
    void saveas();
    void renderGui();
    void treeView();
    glm::mat4 build(buildSetting _settings, bool _addVerts);

    FileDialogFilterVec filters = { {"leaf"} };

private:
    // going to do all this in mm
    float2  stem_length = { 0.f, 0.3f };
    float2  stem_width = { 4.f, 0.f };
    float2  stem_curve = { 0.0f, 0.3f };      // radian bend over lenth
    float2  stem_to_leaf = { 0.0f, 0.2f };      // radian bend over lenth
    int2    stemVerts = { 2, 4 };

    bool    cameraFacing = false;
    float2  leaf_length = { 100.f, 0.2f };
    float2  leaf_width = { 60.f, 0.2f };
    float2  leaf_curve = { 0.0f, 0.2f };      // radian bend over lenth
    float2  leaf_twist = { 0.0f, 0.2f };      // radian bend over lenth
    float2  width_offset = { 1.0f, 0.1f };      // Y value pushes teh leaf edges outwards makign it fatter
    float2  gravitrophy = { 0, 0 };
    int2    numVerts = { 3, 12 };
    float2  perlinCurve = { 0.1f, 4.0f };
    float2  perlinTwist = { 0.1f, 4.0f };

    _vegMaterial stem_Material;
    randomVector<_vegMaterial> materials;

    
public:
    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float2(stem_length);
        archive_float2(stem_width);
        archive_float2(stem_curve);
        archive_float2(stem_to_leaf);
        archive_float2(stemVerts);

        archive(cameraFacing);
        archive_float2(leaf_length);
        archive_float2(leaf_width);
        archive_float2(leaf_curve);
        archive_float2(leaf_twist);
        archive_float2(width_offset);
        archive_float2(gravitrophy);
        archive_float2(numVerts);  // teh define works for ints as well

        archive(stem_Material);
        archive(materials.data); //??? moce to randomVector, keep up to date there? more contained

        if (_version >= 101)
        {
            archive_float2(perlinCurve);
            archive_float2(perlinTwist);
        }

        stem_Material.reload();
        for (auto& M : materials.data) M.reload();

        if (_version >= 102)
        {
            archive(CEREAL_NVP(ossilation_stiffness));
            archive(CEREAL_NVP(ossilation_constant_sqrt));
            archive(CEREAL_NVP(ossilation_power));
            archive(CEREAL_NVP(deepest_pivot_pack_level));
        }
    }
};
CEREAL_CLASS_VERSION(_leafBuilder, 102);




class _stemBuilder : public _plantBuilder
{
public:
    void loadPath();
    void savePath();
    void load();
    void save();
    void saveas();
    void renderGui();
    void treeView();
    lodBake* getBakeInfo(uint i);//bakeInfo
    levelOfDetail* getLodInfo(uint i);
    void build_lod_0(buildSetting _settings);
    void build_lod_1(buildSetting _settings, bool _buildLeaves);
    void build_lod_2(buildSetting _settings, bool _buildLeaves);
    void build_leaves(buildSetting _settings, uint _max, bool _addVerts);
    void build_tip(buildSetting _settings, bool _addVerts);
    void build_extents(buildSetting _settings);
    void build_NODES(buildSetting _settings, bool _addVerts);
    glm::mat4 build(buildSetting _settings, bool _addVerts);
    

    //void build_Nodes(buildSetting& _settings);
    std::vector<glm::mat4> NODES;
    glm::mat4 tip_NODE;
    //std::vector<glm::mat4> NODES_PREV; // bad bad leroy brown do without, so build twice, alwasy
    float age;  // number of segments floatign pouint, after randomize, needed for leaves build
    float tip_width = 0;
    float root_width = 0;
    int     firstLiveSegment = 1;

    FileDialogFilterVec filters = { {"stem"} };

    //Stem has to maintain a minimum of 3 of these or will crash
    std::vector<levelOfDetail> lodInfo = { levelOfDetail(10), levelOfDetail(14), levelOfDetail(40), levelOfDetail(100), levelOfDetail(300), levelOfDetail(500) };
    std::array<lodBake, 3> lod_bakeInfo = { lodBake(32, 1.f), lodBake(64, 0.6f), lodBake(128, 0.2f) };

    // stem
    float2  numSegments = { 5.3f, 0.3f };
    float2  max_live_segments = { 10.f, 0.2f };  // curve for width growth
    float2  stem_length = { 50.f, 0.2f };   // in mm
    float2  stem_width = { 1.f, 0.2f };     // width at the start
    float2  stem_d_width = { 0.1f, 0.1f };  // width growth per node
    float2  stem_pow_width = { 0.1f, 0.1f };  // curve for width growth
    float2  stem_curve = { 0.2f, 0.3f };      // radian bend over lenth
    float2  stem_phototropism = { 0.0f, 0.3f };
    float2  node_rotation = { 0.7f, 0.3f };   // like 2 leaves 90 degrees
    float2  node_angle = float2(0.2f, 0.2f);    // andgle that the stem bends at the node ??? always away fromt he leaf angle if there is such a thing
    _vegMaterial stem_Material;

    

    // branches, can be leavea
    float2  numLeaves = {3.f, 0.f};  // per segment
    float2  leaf_angle = float2(0.5f, 0.3f);   // angle that the leaves come out of
    float2  leaf_rnd = float2(0.3f, 0.3f);
    float   leaf_age_power = 2.f;
    bool    twistAway = false;      // if single leaf, activelt twist stem to the other side
    randomVector<_plantRND> leaves;

    // tip
    bool unique_tip;
    randomVector<_plantRND> tip;

    // feedback
    uint numLeavesBuilt = 0;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float2(numSegments);
        archive_float2(max_live_segments);
        archive_float2(stem_length);
        archive_float2(stem_width);
        archive_float2(stem_d_width);
        archive_float2(stem_pow_width);
        archive_float2(stem_curve);
        archive_float2(stem_phototropism);
        archive_float2(node_rotation);   // around the axis
        archive_float2(node_angle);
        archive(stem_Material);
        stem_Material.reload();

        archive_float2(numLeaves);
       
        archive_float2(leaf_angle);
        archive_float2(leaf_rnd);
        archive(leaf_age_power);
        archive(twistAway);
        archive(leaves.data);
        for (auto& M : leaves.data) M.reload();

        archive(unique_tip);
        archive(tip.data);
        if (unique_tip) {
            for (auto& M : tip.data) M.reload();
        }

        archive(lod_bakeInfo);
        for (auto& M : lod_bakeInfo) M.material.reload();

        archive(CEREAL_NVP(lodInfo));

        archive(CEREAL_NVP(shadowUVScale));
        archive(CEREAL_NVP(shadowSoftness));
        archive(CEREAL_NVP(shadowDepth));
        archive(CEREAL_NVP(shadowPenetationHeight));

        archive(CEREAL_NVP(ossilation_stiffness));
        archive(CEREAL_NVP(ossilation_constant_sqrt));
        archive(CEREAL_NVP(ossilation_power));
        archive(CEREAL_NVP(deepest_pivot_pack_level));
        
    }
};
CEREAL_CLASS_VERSION(_stemBuilder, 100);






class _rootPlant
{
public:
    void onLoad();
    void renderGui(Gui* _gui);
    void buildAllLods();
    void build(bool _updateExtents = false);
    void loadMaterials();
    void reloadMaterials();

    void remapMaterials();
    void import();
    void eXport();

    void bake(std::string _path, std::string _seed, lodBake *_info);
    void render(RenderContext* _renderContext, const Fbo::SharedPtr& _fbo, GraphicsState::Viewport _viewport, Texture::SharedPtr _hdrHalfCopy,
        rmcv::mat4  _viewproj, float3 camPos, rmcv::mat4  _view, rmcv::mat4  _clipFrustum);

    RenderContext* renderContext;

    //??? lodding info, so it needs all the runtime data

    bool displayModeSinglePlant = true;

    int totalInstances = 16384;
    float instanceArea = 40.f;
    bool cropLines = false;
    void builInstanceBuffer();

    _plantBuilder *root = nullptr;
    buildSetting settings;
    bakeSettings bakeSettings;
    float2 extents;


    float3 windDir = {1, 0, 0};
    float windStrength = 1.f;      // m/s

    // render and bake
    Sampler::SharedPtr			sampler_ClampAnisotropic;
    Sampler::SharedPtr			sampler_Ribbons;
    RasterizerState::SharedPtr      rasterstate;
    BlendState::SharedPtr           blendstate;
    BlendState::SharedPtr           blendstateBake;
    pixelShader vegetationShader;
    pixelShader billboardShader;
    pixelShader bakeShader;
    ShaderVar varVegTextures;
    ShaderVar varBBTextures;
    ShaderVar varBakeTextures;

    computeShader		compute_bakeFloodfill;

    Buffer::SharedPtr blockData;
    Buffer::SharedPtr instanceData;
    Buffer::SharedPtr instanceData_Billboards;
    Buffer::SharedPtr plantData;
    Buffer::SharedPtr vertexData;
    std::array<plant, 256> plantBuf;
    std::array<plant_instance, 16384> instanceBuf;
    std::array<block_data, 16384 * 32> blockBuf;
    std::array<ribbonVertex8, 128 * 256> vertexBuf;
    uint totalBlocksToRender = 0;
    uint unusedVerts = 0;
    bool tempUpdateRender = false;
    float gputime;
    float gputimeBB;   // GPU time

    Buffer::SharedPtr  buffer_feedback;
    Buffer::SharedPtr  buffer_feedback_read;
    vegetation_feedback feedback;
    float loddingBias = 1.f;

    Buffer::SharedPtr   drawArgs_vegetation;
    Buffer::SharedPtr   drawArgs_billboards;
    computeShader		compute_clearBuffers;
    computeShader		compute_calulate_lod;




    static _plantBuilder *selectedPart;
    static _plantMaterial *selectedMaterial;

    // randomizer
    static std::mt19937 generator;
    static std::uniform_real_distribution<> rand_1;
    static std::uniform_real_distribution<> rand_01;
    static std::uniform_int_distribution<> rand_int;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
    }
};
CEREAL_CLASS_VERSION(_rootPlant, 100);

