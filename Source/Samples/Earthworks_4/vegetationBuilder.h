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

    int find_insert_material(const std::string _path, const std::string _name);
    int find_insert_material(const std::filesystem::path _path);
    int find_insert_texture(const std::filesystem::path _path, bool isSRGB);
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

    void import(std::filesystem::path _path, bool _replacePath = true);
    void import(bool _replacePath = true);
    void save();
    void eXport(std::filesystem::path _path);
    void eXport();
    void reloadTextures();
    void loadTexture(int idx);

    template<class Archive>
    void serialize(Archive& _archive, std::uint32_t const _version)
    {
        _archive(CEREAL_NVP(albedoName));
        _archive(CEREAL_NVP(alphaName));
        _archive(CEREAL_NVP(normalName));
        _archive(CEREAL_NVP(translucencyName));

        _archive(CEREAL_NVP(albedoPath));
        _archive(CEREAL_NVP(alphaPath));
        _archive(CEREAL_NVP(normalPath));
        _archive(CEREAL_NVP(translucencyPath));

        _archive(CEREAL_NVP(_constData.translucency));
        _archive(CEREAL_NVP(_constData.alphaPow));
    }


    sprite_material _constData;
};
#define _PLANTMATERIALVERSION 100
CEREAL_CLASS_VERSION(_plantMaterial, _PLANTMATERIALVERSION);




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
    glm::mat4   root;
    float       age;
    float       pixelSize;
    int         seed;
};

struct bakeSettings
{
    float objectSize = 4.0f;
    float radiusScale = 1.0f;                   //  so biggest radius
    float3 objectOffset = float3(0.5, 0.1f, 0.5f);
    float getScale() { return objectSize / 16384.0f; }
    float3 getOffset() { return objectOffset * objectSize; }
};



struct ribbonVertex
{
    static float objectScale;   //0.002 for trees  // 32meter block 2mm presision
    static float radiusScale;   //  so biggest radius now objectScale / 2.0f;
    static float O;
    static float3 objectOffset;

    static void setup(float scale, float radius, float3 offset);
    ribbonVertex8 pack();

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
    bool renderGui();

    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(name);
        archive(path);
        archive_float2(albedoScale);
        archive_float2(translucencyScale);
    }
};



class _plantBuilder
{
public:
    void loadPath() { ; }
    void savePath() { ; }
    virtual void load() { ; }
    virtual void save() { ; }
    virtual void saveas() { ; }
    virtual void renderGui(Gui* _gui) { ; }
    virtual void treeView() { ; }
    virtual void build(buildSetting& _settings) { ; }

    std::string name = "not set";
    std::string path = "no path either";   // relative
    bool changed = false;
};






// would be nice if we have more controll over distribution including with age
template <class T> class randomVector 
{
public:
    randomVector() { data.resize(1); }      // minimum size has to be one, otherwise get() is dangerous
    std::vector<T> data;
    
    void renderGui(char *name);
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
    void renderGui(Gui* _gui);
    void treeView();
    void build(buildSetting& _settings);

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
    float2  width_offset = { 1.0f, 0.1f };
    float2  gravitrophy = { 0, 0 };
    int2    numVerts = { 3, 12 };

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

        stem_Material.reload();
        for (auto& M : materials.data) M.reload();
    }
};
CEREAL_CLASS_VERSION(_leafBuilder, 100);




class _twigBuilder : public _plantBuilder
{
public:
    void loadPath();
    void savePath();
    void load();
    void save();
    void saveas();
    void renderGui(Gui* _gui);
    void treeView();
    void build(buildSetting& _settings);

    FileDialogFilterVec filters = { {"twig"} };

    _plantMaterial stemMaterial;
    randomVector<_plantBuilder> leaves;
    randomVector<_plantBuilder> tip;
    randomVector<_plantMaterial> materials;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
    }
};
CEREAL_CLASS_VERSION(_twigBuilder, 100);






class _rootPlant
{
public:
    void renderGui(Gui* _gui);
    void build(glm::mat4 root, float _age, float _lodPixelsize, int _seed);
    void loadMaterials();
    void reloadMaterials();

    void remapMaterials();
    void import();
    void eXport();

    void bake(RenderContext* _renderContext);

    //??? lodding info, so it needs all the runtime data

    _plantBuilder *root = nullptr;

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
        //archive( cereal::base_class<_plantBuilder>( this ), y );
        reloadMaterials();
        rand_1.
    }
};
CEREAL_CLASS_VERSION(_rootPlant, 100);

