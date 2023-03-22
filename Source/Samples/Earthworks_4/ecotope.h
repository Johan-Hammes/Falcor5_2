#pragma once

#include "Falcor.h"
using namespace Falcor;
#include"hlsl/groundcover_defines.hlsli"
#include"hlsl/terrainDefines.hlsli"
#include"hlsl/gpuLights_defines.hlsli"

#include "cereal/cereal.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"





// ??? fixcme move to one of the slang define files
struct _GPU_ecotopePlant
{
	uint		idx;
	uint		lod;
	float		density;
	float		scale;		// ???
	float		size;		// I have to pack this better foir GPU damn ;-) 

	//float		scaleVariation;	  // not gettign used now, but maybe keep - packs better without
};

// not great  but packs into GPU mem ok'ish
// should expand to bump / terrafector later
// maybe this should become 2 buffers to pack tighter
struct _GPU_ecotope
{
	uint	numplants = 0;
	uint	X1;			//??? can we use any other info
	uint	X2;
	uint	X3;
	_GPU_ecotopePlant	plants[256];
	uint	plantLOD[8192];
	float	plantSize[8192];
};


/*	This is incomplete, sort of placeholder till I work out the rendering part*/
struct _displayPlant
{
	float				density = 10.0f;		  // redefine relative to size - think about it
	float				scale = 1.0f;
	float				scaleVariation = 0.3f;
	std::string			name;
	std::string			shortName;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(density));
		archive(CEREAL_NVP(scale));
		archive(CEREAL_NVP(scaleVariation));
		archive(CEREAL_NVP(name));
	}
};



class ecotope {
public:
	ecotope();
	virtual ~ecotope() { ; }

	void loadTexture(int i);
	void reloadTextures();

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(name));
		archive(CEREAL_NVP(weights));
		archive(CEREAL_NVP(albedoName), CEREAL_NVP(noiseName), CEREAL_NVP(displacementName), CEREAL_NVP(roughnessName), CEREAL_NVP(aoName));
		archive(CEREAL_NVP(plants));
	}
	void GuiTextures(Gui *_pGui);
	void GuiWeights(Gui *_pGui);
	void GuiPlants(Gui *_pGui);
	void renderGUI(Gui *_pGui);
	void renderPlantGUI(Gui *_pGui);
	uint selectedPlant = 0;

	void addPlant();

	std::string name = "new ecotope";
	std::array<std::array<float, 4>, 6> weights;	  // maps to 5 float4's but better for cereal to stay STL and do memcpy - waste space for consistency

	std::string albedoName;
	std::string noiseName;
	std::string displacementName;
	std::string roughnessName;
	std::string aoName;

	std::vector<_displayPlant> plants;

	Texture::SharedPtr	texAlbedo;
	Texture::SharedPtr	texNoise;
	Texture::SharedPtr	texDisplacement;
	Texture::SharedPtr	texRoughness;
	Texture::SharedPtr	texAO;
};


class ecotopeSystem {
public:
	ecotopeSystem() { ; }
	virtual ~ecotopeSystem() { ; }

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(ecotopes));
	}

	void load();
	void save();
	void renderGUI(Gui *_pGui);
	void renderSelectedGUI(Gui *_pGui);
	void renderPlantGUI(Gui *_pGui);
	void addEcotope();
	

	std::vector<ecotope>	ecotopes;		// FIXME LIST

	int selectedEcotope = 0;

	// runtime
    Buffer::SharedPtr constantBuffer;
	void rebuildRuntime();
};

