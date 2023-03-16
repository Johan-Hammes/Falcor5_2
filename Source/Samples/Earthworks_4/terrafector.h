#pragma once



#include "Falcor.h"		// for glm includes float4 etc
#include<unordered_map>
#include<list>

#include "cereal/archives/binary.hpp"
#include "cereal/archives/xml.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/cereal.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include <fstream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//#include "ecotope.h"



using namespace Falcor;
#include"hlsl/groundcover_defines.hlsli"
#include"hlsl/terrainDefines.hlsli"
#include"hlsl/gpuLights_defines.hlsli"
#include"hlsl/materials.hlsli"


#define archive_float2(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y));}
#define archive_float3(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z));}
#define archive_float4(v) {archive(CEREAL_NVP(v.x)); archive(CEREAL_NVP(v.y)); archive(CEREAL_NVP(v.z)); archive(CEREAL_NVP(v.w));}


// FIXME move to hlsl
class  triVertex {
public:
    glm::float3 pos;
    float       alpha;      // might have to be full float4 colour   but look at float16

    glm::float2 uv;         // might have to add second UV
    uint        material;
    float       buffer;         // now 32

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive_float3(pos);
        archive_float2(uv);
        archive(CEREAL_NVP(material));
        archive(CEREAL_NVP(alpha));
    }
};
CEREAL_CLASS_VERSION(triVertex, 100);



class triangleBlock {
public:
private:
    std::array<glm::int4, 128> index;
};

class tileTriangleBlock {
public:
    void clear();
    void clearRemapping(uint _size);
    void remapMaterials(uint* _map);
    void insertTriangle(const uint material, const uint F[3], const aiMesh* _mesh);
private:
public:
    std::vector<triVertex> verts;
    std::vector<int> remapping;
    std::vector<triangleBlock> indexBlocks;
    std::vector<uint> tempIndexBuffer;

    uint vertexReuse;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(verts));
        archive(CEREAL_NVP(tempIndexBuffer));
    }
};
CEREAL_CLASS_VERSION(tileTriangleBlock, 100);



class lodTriangleMesh {
public:
    void create(uint _lod);
    void remapMaterials(uint* _map);
    void prepForMesh(aiAABB _aabb, uint _size, std::string _name);
    int insertTriangle(const uint material, const uint F[3], const aiMesh* _mesh);
    void logStats();
    void save(const std::string _path);
    bool load(const std::string _path);
    

private:
    uint xMin, xMax, yMin, yMax;
    uint lod;
    uint grid;
    float tileSize;
    float bufferSize;
    
public:
    std::vector<tileTriangleBlock> tiles;
    std::vector<std::string> materialNames;

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const _version)
    {
        archive(CEREAL_NVP(lod));
        archive(CEREAL_NVP(materialNames));
        archive(CEREAL_NVP(tiles));
    }
};
CEREAL_CLASS_VERSION(lodTriangleMesh, 100);



struct gpuTileTerrafector
{
    uint numVerts = 0;
    uint numTriangles = 0;
    uint numBlocks = 0;
    Buffer::SharedPtr vertex = nullptr;
    Buffer::SharedPtr index = nullptr;
};



class lodTriangleMesh_LoadCombiner {
public:
    void create(uint _lod);
    void addMesh(std::string _path, lodTriangleMesh &mesh);
    void loadToGPU();
    gpuTileTerrafector* getTile(uint _index) {
        if (_index < gpuTiles.size()) {
            return &gpuTiles[_index];
        }
        return nullptr;
    }

private:
    uint lod, grid;
    std::vector<tileTriangleBlock> tiles;
    std::vector <gpuTileTerrafector> gpuTiles;
};


class terrafectorEditorMaterial;

class materialCache {
public:
    materialCache();
	virtual ~materialCache() { ; }

	uint find_insert_material(const std::string _path, const std::string _name);
    uint find_insert_material(const std::filesystem::path _path);
	uint find_insert_texture(const std::filesystem::path _path, bool isSRGB);

	void setTextures(ShaderVar& var);

	void rebuildStructuredBuffer();
	void rebuildAll();
    //void resolveSubMaterials();

	std::vector<terrafectorEditorMaterial>	materialVector;
	int selectedMaterial = -1;
	std::vector<Texture::SharedPtr>			textureVector;
	float texMb = 0;

	void renderGui(Gui *mpGui, Gui::Window& _window);
    void renderGuiTextures(Gui* mpGui, Gui::Window& _window);
	bool renderGuiSelect(Gui *mpGui);
    void reFindMaterial(std::filesystem::path currentPath);
    void renameMoveMaterial(terrafectorEditorMaterial& _material);

	int dispTexIndex = -1;
	Texture::SharedPtr getDisplayTexture();

    Buffer::SharedPtr sb_Terrafector_Materials = nullptr;
    std::string rootMaterialPath = "X:/resources/terrafectors_and_road_materials/roads/";
};



class terrafectorEditorMaterial {
public:
	terrafectorEditorMaterial();
	virtual ~terrafectorEditorMaterial();


	static materialCache static_materials;

	uint32_t blendHash();

	void import(std::filesystem::path _path, bool _replacePath = true);
	void import(bool _replacePath = true);
    void save();
	void eXport(std::filesystem::path _path);
	void eXport();
	void reloadTextures();
	void loadTexture(int idx);
    void loadSubMaterial(int idx);
    void clearSubMaterial(int idx);

	bool renderGUI(Gui *mpGui);

	static std::string rootFolder;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(displayName));

		archive(CEREAL_NVP(useAbsoluteElevation));

		archive(CEREAL_NVP(texturePaths));
		archive(CEREAL_NVP(textureNames));

        archive(CEREAL_NVP(submaterialPaths));

		// structured buffer data - although texture pointers are incomplete
		archive(CEREAL_NVP(_constData));
		_constData.useAbsoluteElevation = useAbsoluteElevation;

		//reloadTextures();		// argues for seperate load and save
		//rebuildBlendstate();
		//rebuildConstantbuffer();
	}

	// should maybe be done with friend
public:
	std::string			  displayName = "Not set!";			
    std::filesystem::path 			  fullPath;			// for quick save
	bool				isModified = false;
    std::array<std::string, 8>	  submaterialPaths = { "", "", "", "", "", "", "", "" };
    Texture::SharedPtr		thumbnail = nullptr;

	bool			useAbsoluteElevation = true;	// deprecated

	enum tfTextures { baseAlpha, detailAlpha, baseElevation, detailElevation, baseAlbedo, detailAlbedo, baseRoughness, detailRoughness, ecotope, tfTextureSize };
	std::array<std::string, 9> tfElement = { "##baseAlpha", "##detailAlpha", "##baseElevation", "##detailElevation", "##baseAlbedo", "##detailAlbedo", "##baseRoughness", "##detailRoughness", "##ecotope" };
	std::array<bool, 9> tfSRGB = { false, false, false, false, true, true, false, false, false };

	std::array<std::string, 9>	texturePaths;
	std::array<std::string, 9>	textureNames;

	//int overlayTextureIndex = -1;


	enum tfMaterialTypes { tfmat_standard, tfmat_rubber, tfmat_puddle, tfmat_legacyRubber };
/*
	struct {
		float	reflectance = 0;
		float	microFiber = 0;
		float	microShadow = 0;
		float	lightWrap = 0;
	} _materialFixedData;
    */

	struct {
		int	materialType = 0;
		float2  uvScale = {1.0f, 1.0f};
		float	worldSize = 4.0f;

		float2  uvScaleClampAlpha = { 1.0f, 1.0f };
		uint	debugAlpha = 0;
		float	uvRotation = 0.0f;

		float	useAlpha = 1;				// bool
		float	vertexAlphaScale = 0.0f;
		float	baseAlphaScale = 0.0f;
		float	detailAlphaScale = 0.0f;

		uint	baseAlphaTexture = 0;
		float	baseAlphaBrightness = 0.0f;
		float	baseAlphaContrast = 1.0f;
		uint	baseAlphaClampU = 0;		// bool

		uint	detailAlphaTexture = 0;
		float	detailAlphaBrightness = 0.0f;
		float	detailAlphaContrast = 1.0f;
		float	useAbsoluteElevation;

		uint	useElevation = 0;
		float	useVertexY;
		float	YOffset;
		uint	baseElevationTexture;

		float	baseElevationScale = 0.0f;
		float	baseElevationOffset = 0.5f;
		uint	detailElevationTexture;
		float	detailElevationScale = 0.0f;

		float	detailElevationOffset = 0.5f;
		float3	buf_____02;

		int		standardMaterialType = 0;		// mircrowshadow etc
		float3	buf_____03;

		uint	useColour = 0;
		uint	baseAlbedoTexture;
		float	albedoBlend;
		uint	detailAlbedoTexture;

		float3	albedoScale = {0.5f, 0.5f, 0.5f};
		float	uvWorldRotation = 0.0f;

		uint	baseRoughnessTexture;
		float	roughnessBlend;
		uint	detailRoughnessTexture;
		float	roughnessScale = 1.0f;

		float	porosity = 0.5f;
		float3	buf_____05;

		uint	useEcotopes = 0;
		float	permanenceElevation = 0;
		float	permanenceColour = 0;
		float	permanenceEcotopes = 0;

		float	cullA = 0;
		float	cullB = 0;
		float	cullC = 0;
		uint	ecotopeTexture;

        std::array<uint, 8>	subMaterials = {0, 0, 0, 0, 0, 0, 0, 0};

		std::array<float4, 15>	ecotopeMasks;

		template<class Archive>
		void serialize(Archive & archive)
		{
			
			archive(CEREAL_NVP(materialType));
			archive(CEREAL_NVP(uvScale.x), CEREAL_NVP(uvScale.y));
			archive(CEREAL_NVP(worldSize));
            archive(CEREAL_NVP(uvRotation));
            archive(CEREAL_NVP(uvWorldRotation));

			archive(CEREAL_NVP(uvScaleClampAlpha.x), CEREAL_NVP(uvScaleClampAlpha.y));

			archive(CEREAL_NVP(useAlpha), CEREAL_NVP(vertexAlphaScale), CEREAL_NVP(baseAlphaScale), CEREAL_NVP(detailAlphaScale));
			archive(CEREAL_NVP(baseAlphaTexture), CEREAL_NVP(baseAlphaBrightness), CEREAL_NVP(baseAlphaContrast), CEREAL_NVP(baseAlphaClampU));
			archive(CEREAL_NVP(detailAlphaTexture), CEREAL_NVP(detailAlphaBrightness), CEREAL_NVP(detailAlphaContrast));
			archive(CEREAL_NVP(useElevation), CEREAL_NVP(useVertexY), CEREAL_NVP(YOffset), CEREAL_NVP(baseElevationTexture));
			archive(CEREAL_NVP(baseElevationScale), CEREAL_NVP(baseElevationOffset), CEREAL_NVP(detailElevationTexture), CEREAL_NVP(detailElevationScale));
			archive(CEREAL_NVP(detailElevationOffset));
			archive(CEREAL_NVP(standardMaterialType));
			archive(CEREAL_NVP(useColour), CEREAL_NVP(baseAlbedoTexture), CEREAL_NVP(albedoBlend), CEREAL_NVP(detailAlbedoTexture));
			archive(CEREAL_NVP(albedoScale.x), CEREAL_NVP(albedoScale.y), CEREAL_NVP(albedoScale.z));
			archive(CEREAL_NVP(baseRoughnessTexture), CEREAL_NVP(roughnessBlend), CEREAL_NVP(detailRoughnessTexture), CEREAL_NVP(roughnessScale));
			archive(CEREAL_NVP(porosity));
			archive(CEREAL_NVP(useEcotopes), CEREAL_NVP(permanenceElevation), CEREAL_NVP(permanenceColour), CEREAL_NVP(permanenceEcotopes));
			archive(CEREAL_NVP(cullA), CEREAL_NVP(cullB), CEREAL_NVP(cullC), CEREAL_NVP(ecotopeTexture));

            archive(CEREAL_NVP(subMaterials));

			for (int i = 0; i < 15; i++) {
				archive(CEREAL_NVP(ecotopeMasks[i].x), CEREAL_NVP(ecotopeMasks[i].y), CEREAL_NVP(ecotopeMasks[i].z), CEREAL_NVP(ecotopeMasks[i].w));
			}

			
			
		}
	} _constData;		// 464 bytes for now


	// the runtime section
	// it makes no sense splitting this for an editor, just confuses the code - write with cereal - have a converter to protobuffers, and redo this code in EVO
	// that way at least there is a path for the editing to migrate to EVO
	void rebuildConstantbuffer();
	void rebuildConstantbufferData();
	//Texture::SharedPtr			pTexture[9];			// FIXME - these happen on a higer terrafector level - the just set an index here
	int textureIndex[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
	BlendState::SharedPtr		blendstate;
	//Buffer::SharedPtr	constantBuffer;  FIXME ADD BACK
};
CEREAL_CLASS_VERSION(terrafectorEditorMaterial, 101);



enum tfTypes { tf_heading, tf_fbx, tf_triMesh, tf_bezier, tf_quad };
class terrafectorElement
{
public:
	terrafectorElement(tfTypes _t, const std::string _name) : type(_t), name(_name) { ; }
	virtual ~terrafectorElement() { ; }

	//private:
	tfTypes type;
	std::string name;
	std::string path;
	std::vector<terrafectorElement> children;

    bool isMeshCached(const std::string _path);
    void splitAndCacheMesh(const std::string _path);
	terrafectorElement &find_insert(const std::string _name, const tfTypes _type= tf_heading, const std::string _path="");
	void renderGui(Gui* _pGui, float tab = 0);

    void loadPath(std::string _path);

	TriangleMesh::SharedPtr		pMesh;
	struct subMesh {
		uint index;
		uint sortVal;
		uint sortIndex;
		std::string materialName;
	};
	std::vector<subMesh>	materials;
	bool visible = true;
	bool bExpanded = false;
    bool bakeOnly = false;

    
}; 





class terrafectorSystem
{
public:
    terrafectorSystem() { ;	}
	virtual ~terrafectorSystem() { ; }

	void loadFile();
	void saveFile();
	template<class Archive>
	void serialize(Archive & archive) {	/*archive(groups); */}

	void renderGui(Gui* mpGui, Gui::Window& _window);
    void loadPath(std::string _path, bool _rebuild = false);

public:

	static bool needsRefresh;
	//static ecotopeSystem *pEcotopes;
	static FILE *_logfile;
	terrafectorElement root = terrafectorElement(tf_heading, "root");

    static lodTriangleMesh_LoadCombiner loadCombine_LOD2;       // will only be used if flagged by artists - large ecotopes only - roughly 40 -> 20 meter pixels this is the far horizon
    static lodTriangleMesh_LoadCombiner loadCombine_LOD4;       // 10m pixel 2.5km tile
    static lodTriangleMesh_LoadCombiner loadCombine_LOD6;       // 2.5m pixel 600m tile

    static lodTriangleMesh_LoadCombiner loadCombine_LOD4_bakeLow;       // all baking will happen at lod4 level
    static lodTriangleMesh_LoadCombiner loadCombine_LOD4_bakeHigh;
    static lodTriangleMesh_LoadCombiner loadCombine_LOD4_overlay;
};

