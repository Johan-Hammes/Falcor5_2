


// We have a large linear array of this, that gets filled from compute
struct block_data
{
    uint instance_idx;          // -> plant     20 bits at least
    uint section_idx;           // -> plant_section - xpbd sim
    uint vertex_offset;             // -> ribbonVertex8 data in 32 vertex blocks for now
    uint plant_idx;             // can find this via instance but likely faster and keeps us aligned    16 bits enough
};


struct plant_instance
{
    uint plant_idx;
    float time_offset;  // for animation
    float scale;
    float rotation;

    float3 position;

    // lights
    
    // xpbd data, sub parts I guess
    
    // I think wind vector should live in an external system and run as a lookup
};


struct plant_section
{
    // sub parts for xpbd
};

struct _plant_lod
{
    float pixSize;
    uint startVertex;
    uint numBlocks;
    uint reserved; 
};

struct plant
{
    float2 size;        // half width but full height

    // packing
    float scale;
    float radiusScale;
    float3 offset;
    float unused_01;

    // lighting
    float Ao_depthScale = 0.3f;
    float sunTilt = -0.2f;
    float bDepth = 20.0f;
    float bScale = 0.5f;

    // flutter
    float flutter_stength;
    float flutter_freq;

    // lodding
    _plant_lod lods[16];    // 16 should do, has t be fixed, 8 might also bnit careful for big trees, although they should really sub-lod
    uint numLods;
    uint billboardMaterialIndex;
};


struct ribbonVertex8
{
    uint a;
    uint b;
    uint c;
    uint d;
    uint e;
    uint f;
    //uint g;
    //uint h;
};


#define VEG_BLOCK_SIZE 32
