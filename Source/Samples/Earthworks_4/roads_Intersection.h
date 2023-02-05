#pragma once

#include "Falcor.h"
using namespace Falcor;

#include "cereal/cereal.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/list.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"


#include"roads_road.h"


enum typeOfCorner { automatic, radius, artistic };
class roadSection;
class intersectionRoadLink {
public:
    bool operator < (const intersectionRoadLink& str) const
    {
        return (angle < str.angle);
    }

    float angle;
    roadSection* roadPtr;
    uint roadGUID;
    bool broadStart;	// are we attached to the start, if false the end

    float pushBack_A;
    float pushBack_B;
    float pushBack;
    float3 cornerUp_A;
    float3 cornerUp_B;

    float3 corner_A;
    float3 corner_B;
    float3 cornerTangent_A;
    float3 cornerTangent_B;

    float3 tangentVector;			// from the center at the road connection angle, and perpendicular to the intersection andchorNormal

    int cornerType = typeOfCorner::automatic;
    float cornerRadius = 5.0f;
    bool bOpenCorner;
    float theta; // the angle of this corner

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        archive(CEREAL_NVP(angle));
        archive(CEREAL_NVP(roadGUID));
        archive(CEREAL_NVP(broadStart));
        archive_float3(corner_A);
        archive_float3(cornerTangent_A);
        archive_float3(corner_B);
        archive_float3(cornerTangent_B);
        archive_float3(tangentVector);
        archive(CEREAL_NVP(cornerType));
        archive(CEREAL_NVP(cornerRadius));
    }
};
CEREAL_CLASS_VERSION(intersectionRoadLink, 100);



class intersection {
public:
    intersection() { ; }
    virtual ~intersection() { ; }

    glm::vec3 anchor;
    glm::vec3 getAnchor() { return (anchor + glm::vec3(0, heightOffset, 0)); }
    glm::vec3 anchorNormal;
    float heightOffset = 0.0f;
    bool customNormal;
    int GUID = 0;

    int buildQuality = 0;
    uint lidarLOD = 0;
    bool doNotPush = false;

    intersectionRoadLink* findLink(int roadGUID);
    void clearRoads();
    bool isClose(glm::vec3 P, float dist = 10.0f);
    void updatePosition(float3 _pos, float3 _normal, uint _lod);
    void updateCorner(float3 pos, float3 normal, int idx, int flags);
    std::vector<intersectionRoadLink> roadLinks;

    std::vector<cubicDouble> lanesTarmac;
    void convertToGPU(uint* totalBezierCount, bool _forExport = false);
    void updateTarmacLanes();

    template<class Archive>
    void serialize(Archive& archive, std::uint32_t const version)
    {
        if (version >= 100)
        {
            archive(CEREAL_NVP(GUID));
            archive_float3(anchor);
            archive_float3(anchorNormal);
            archive(CEREAL_NVP(roadLinks));
            archive(CEREAL_NVP(buildQuality));
            archive(CEREAL_NVP(heightOffset));
            archive(CEREAL_NVP(customNormal));
            archive(CEREAL_NVP(lidarLOD));
            archive(CEREAL_NVP(doNotPush));
        }
        if (version >= 101)
        {
            archive(CEREAL_NVP(lanesTarmac));
        }
    }
};
CEREAL_CLASS_VERSION(intersection, 101);

