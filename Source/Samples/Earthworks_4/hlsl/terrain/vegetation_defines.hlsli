


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
    float3 position;
    float scale;

    float rotation;
    float time_offset; // for animation but not being used for now
    uint plant_idx;
    
    

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

struct _plant_anim_pivot
{
    float3 root;
    float frequency; // scale inside the shader by sqrt(1/scale) as well ALL Frequencies

    float3 extent;  // vector towards the tip but lenght = 1/length - dot product with actual vertex is already on 0..1
    float stiffness;

    float3 relative;    // for the solve DEPRECATED
    float shift; // shifts the ben towards the root abort tip DEPRECATED - new bezier doesnt allow for this

    //???
    //??? should bwe add a pow factor to shift the bend more towards or awasy from teh origin, could be extremely poiwerful, test first

    int offset; // time offset


    /*
        can we pack this tighter
        half3   root, extent
        16  frequency
        8/8 stiffness and offset    128bits 16 bytes   vs 36 bytes badly aligned 
    */
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
    float Ao_depthScale = 0.3f; //??? unused
    float sunTilt = -0.2f;
    float bDepth = 20.0f;
    float bScale = 0.5f;

    // soft shadows
    float shadowUVScale = 1.f;
    float shadowSoftness = 0.15f;
    
    // flutter
    float flutter_stength;
    float flutter_freq;

    // lodding
    _plant_lod lods[16];    // 16 should do, has t be fixed, 8 might also bnit careful for big trees, although they should really sub-lod
    uint numLods;
    uint billboardMaterialIndex;

    // pivot points for animation
    _plant_anim_pivot rootPivot;
    //_plant_anim_pivot pivots[256]; oWNE buggfer he have alimit on strucure size of 2048 bytes
};


struct ribbonVertex8
{
    uint a;
    uint b;
    uint c;
    uint d;
    uint e;
    uint f;
    uint g;
    uint h;
};


#define VEG_BLOCK_SIZE 32



struct vegetation_feedback
{
    // plant zero
    float plantZero_pixeSize;
    uint plantZeroLod;
    uint numBillboard;
    uint numPlant;

    uint numLod[32];

    uint numBlocks;
    uint numInstanceAddedComputeClipLod;
    uint numFrustDiscard;
};
