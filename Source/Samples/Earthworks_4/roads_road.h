#pragma once

#include"roads_bezier.h"
#include"roads_materials.h"



enum typeOfCorner { automatic, radius, artistic };
class roadSection;
class intersection;



class intersectionRoadLink
{
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



struct splineTest
{
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



struct splineUV
{
    float3 pos;
    float U;
    float V;
    glm::vec3 dU;
    glm::vec3 dV;
    glm::vec3 N;			// ??? maybe textyure dep[endent
    glm::vec3 E1;
    glm::vec3 E2;
};



class roadSection
{
public:
    roadSection() { ; }
    virtual ~roadSection() { ; }

    void convertToGPU_Realistic(std::vector<cubicDouble> &_bezier, std::vector<bezierLayer> &_index, std::vector<bezierLayer>& _index_BAKE, uint _from = 99999, uint _to = 88888, bool _stylized = false, bool _showMaterials = true);
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
    intersectionRoadLink* startLink = nullptr;      // FIXME solve these after load
    intersectionRoadLink* endLink = nullptr;

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







