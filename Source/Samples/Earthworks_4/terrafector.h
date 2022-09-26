#pragma once



#include "Falcor.h"		// for glm includes float4 etc
#include<unordered_map>
#include<list>

#include "cereal/cereal.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"

//#include "ecotope.h"



using namespace Falcor;
#include"groundcover_defines.hlsli"
#include"terrainDefines.hlsli"
#include"gpuLights_defines.hlsli"
#include"materials.hlsli"






class terrafectorEditorMaterial;

class materialCache {
public:
    materialCache();
	virtual ~materialCache() { ; }

	uint find_insert_material(const std::filesystem::path _path);
	uint find_insert_texture(const std::filesystem::path _path, bool isSRGB);

	void setTextures(GraphicsVars::SharedPtr _pVars);

	void rebuildStructuredBuffer();
	void rebuildAll();

	std::vector<terrafectorEditorMaterial>	materialVector;
	int selectedMaterial = -1;
	std::vector<Texture::SharedPtr>			textureVector;
	float texMb = 0;

	void renderGui(Gui *mpGui);
	bool renderGuiSelect(Gui *mpGui);

	int dispTexIndex = -1;
	Texture::SharedPtr getDisplayTexture();

    Buffer::SharedPtr sb_Terrafector_Materials = nullptr;
};



class terrafectorEditorMaterial {
public:
	terrafectorEditorMaterial();
	virtual ~terrafectorEditorMaterial();


	static materialCache static_materials;

	uint32_t blendHash();

	void import(std::filesystem::path _path, bool _replacePath = true);
	void import(bool _replacePath = true);
	void eXport(std::filesystem::path _path);
	void eXport();
	void reloadTextures();
	void loadTexture(int idx);

	bool renderGUI(Gui *mpGui);

	static std::string rootFolder;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(displayName));

		archive(CEREAL_NVP(useAbsoluteElevation));

		archive(CEREAL_NVP(texturePaths));
		archive(CEREAL_NVP(textureNames));

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

	bool			useAbsoluteElevation = true;	// deprecated

	enum tfTextures { baseAlpha, detailAlpha, baseElevation, detailElevation, baseAlbedo, detailAlbedo, baseRoughness, detailRoughness, ecotope, tfTextureSize };
	std::array<std::string, 9> tfElement = { "##baseAlpha", "##detailAlpha", "##baseElevation", "##detailElevation", "##baseAlbedo", "##detailAlbedo", "##baseRoughness", "##detailRoughness", "##ecotope" };
	std::array<bool, 9> tfSRGB = { false, false, false, false, true, true, false, false, false };

	std::array<std::string, 9>	texturePaths;
	std::array<std::string, 9>	textureNames;

	//int overlayTextureIndex = -1;


	enum tfMaterialTypes { tfmat_standard, tfmat_rubber, tfmat_puddle, tfmat_legacyRubber };

	struct {
		float	reflectance;
		float	microFiber;
		float	microShadow;
		float	lightWrap;
	} _materialFixedData;


	struct {
		int	materialType = 0;
		float2  uvScale = {1.0f, 1.0f};
		float	worldSize = 4.0f;

		float2  uvScaleClampAlpha = { 1.0f, 1.0f };
		uint	debugAlpha = 0;
		float	buf_____06;

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
		float	buf_____04;

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

		std::array<float4, 15>	ecotopeMasks;

		template<class Archive>
		void serialize(Archive & archive)
		{
			
			archive(CEREAL_NVP(materialType));
			archive(CEREAL_NVP(uvScale.x), CEREAL_NVP(uvScale.y));
			archive(CEREAL_NVP(worldSize));

			//if (version >= 101) {
				archive(CEREAL_NVP(uvScaleClampAlpha.x), CEREAL_NVP(uvScaleClampAlpha.y));
			//}

			archive(CEREAL_NVP(useAlpha), CEREAL_NVP(vertexAlphaScale), CEREAL_NVP(baseAlphaScale), CEREAL_NVP(detailAlphaScale));
			archive(CEREAL_NVP(baseAlphaTexture), CEREAL_NVP(baseAlphaBrightness), CEREAL_NVP(baseAlphaContrast), CEREAL_NVP(baseAlphaClampU));
			archive(CEREAL_NVP(detailAlphaTexture), CEREAL_NVP(detailAlphaBrightness), CEREAL_NVP(detailAlphaContrast));
			//if (version >= 102) {
			//	archive(CEREAL_NVP(useAbsoluteElevation));
			//}
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
			for (int i = 0; i < 15; i++) {
				archive(CEREAL_NVP(ecotopeMasks[i].x), CEREAL_NVP(ecotopeMasks[i].y), CEREAL_NVP(ecotopeMasks[i].z), CEREAL_NVP(ecotopeMasks[i].w));
			}

			
			
		}
	} _constData;		// 464 bytes for now

	

	// the runtime section
	// it makes no sense splitting this for an editor, just confuses the code - write with cereal - have a converter to protobuffers, and redo this code in EVO
	// that way at least there is a path for the editing to migrate to EVO
	void rebuildBlendstate();
	void rebuildConstantbuffer();
	void rebuildConstantbufferData();
	//Texture::SharedPtr			pTexture[9];			// FIXME - these happen on a higer terrafector level - the just set an index here
	int textureIndex[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
	BlendState::SharedPtr		blendstate;
	//Buffer::SharedPtr	constantBuffer;  FIXME ADD BACK
};
CEREAL_CLASS_VERSION(terrafectorEditorMaterial, 102);



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

	terrafectorElement &find_insert(const std::string _name, const tfTypes _type= tf_heading, const std::string _path="");
	void renderGui(Gui* _pGui, float tab = 0);
	void render(RenderContext::SharedPtr _pRenderContext, Camera::SharedPtr _pCamera, GraphicsState::SharedPtr _pState, GraphicsVars::SharedPtr _pVars);

	//Model::SharedPtr		pMesh;
	struct subMesh {
		uint index;
		uint sortVal;
		uint sortIndex;
		std::string materialName;
	};
	std::vector<subMesh>	materials;
	//std::vector<uint>		materialsIndex;
	//std::vector<uint>		materialsIndexSort;
	//std::vector<uint>		materialsSort;
	bool visible = true;
	bool bExpanded = false;
}; 





class terrafectorSystem
{
public:
	terrafectorSystem() {
		roads.find_insert("root");
		roads.find_insert("markings");
		roads.find_insert("lanes");
		roads.find_insert("patches");
		roads.find_insert("curvature");
	}
	virtual ~terrafectorSystem() { ; }


	void loadFile();
	void saveFile();
	template<class Archive>
	void serialize(Archive & archive) {	/*archive(groups); */}

	void renderGui(Gui* mpGui);
	void render(RenderContext::SharedPtr _pRenderContext, Camera::SharedPtr _pCamera, GraphicsState::SharedPtr _pState, GraphicsVars::SharedPtr _pVars);



public:

	static bool needsRefresh;
	//static ecotopeSystem *pEcotopes;
	static GraphicsProgram::SharedPtr		topdownProgramForBlends;
	static FILE *_logfile;


	terrafectorElement base = terrafectorElement(tf_heading, "base");
	terrafectorElement roads = terrafectorElement(tf_heading, "roads");
	terrafectorElement top = terrafectorElement(tf_heading, "top");
	terrafectorElement &getRoad(const std::string name) { return roads.find_insert(name); }
};

