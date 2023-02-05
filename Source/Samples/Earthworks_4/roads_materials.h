#pragma once

#include "terrafector.h"



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


static roadMaterialCache roadMatCache;
