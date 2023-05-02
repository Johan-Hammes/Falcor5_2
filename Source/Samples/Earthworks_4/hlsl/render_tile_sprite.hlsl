
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		


static const float size[256] = {
    20, 30, 30, 30, 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    20, 30, 40, 30, 30, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0.1f, 1, 1, 1, 1, 1,
    0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


SamplerState gSampler;
Texture2D gAlbedo : register(t0);
//Texture2DArray gNorm : register(t1);
//Texture2DArray gTranclucent : register(t2);
//TextureCube gCube;


StructuredBuffer<instance_PLANT>        instanceBuffer;
RWStructuredBuffer<gpuTile> 			tiles;
StructuredBuffer<tileLookupStruct>		tileLookup;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
	float3 	right;
	int		alpha_pass;
};



struct VSOut
{
    float4 rootPos : ANCHOR;
    float scale : TEXCOORD;
    uint index : TEXCOORD1;
};

struct GSOut
{
    float4 pos : SV_POSITION;
    float3 texCoords : TEXCOORD;
    //float3 sun : TEXCOORD1;
    //float4 ambientOcclusion : TEXCOORD2;
};


VSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    VSOut output = (VSOut)0;

    uint tileIDX = tileLookup[iId].tile & 0xffff;
    uint numQuad = tileLookup[iId].tile >> 16;
    uint blockID = vId;// (iId & 0x3f);

    // FIXME just fill the rest of the 64 with NULL and render all much faster than this if
    if (blockID < numQuad)
    {
        uint newID = tileLookup[iId].offset + blockID;
        instance_PLANT plant = instanceBuffer[newID];

        output.rootPos = float4(unpack_pos(plant.xyz, tiles[tileIDX].origin, tiles[tileIDX].scale_1024), 1);
        output.scale = SCALE(plant.s_r_idx);
        output.index = PLANT_INDEX(plant.s_r_idx);
    }
    return output;
}



[maxvertexcount(4)]
void gsMain(point VSOut sprite[1], inout TriangleStream<GSOut> OutputStream)
{
    GSOut v;

    uint I = sprite[0].index;
    int dx = I & 0xf;
    int dy = I >> 4;
    float SZ = sprite[0].scale * size[I];

    //create sprite quad
    //--------------------------------------------
    float3 texRoot = float3(dx, dy, 0) * 0.0625;

    //bottom left
    v.pos = mul(sprite[0].rootPos - float4(right, 0)*0.5* SZ, viewproj);
    v.texCoords = texRoot + float3(0.1, 0.9, 0) * 0.0625;
    OutputStream.Append(v);

    //top left
    v.pos = mul(sprite[0].rootPos - float4(right, 0) * 0.5* SZ + float4(0, SZ, 0, 0), viewproj);
    v.texCoords = texRoot + float3(0.1, 0.1, 0) * 0.0625;
    OutputStream.Append(v);

    //bottom right
    v.pos = mul(sprite[0].rootPos + float4(right, 0) * 0.5* SZ, viewproj);
    v.texCoords = texRoot + float3(0.9, 0.9, 0) * 0.0625;
    OutputStream.Append(v);

    //top right
    v.pos = mul(sprite[0].rootPos + float4(right, 0) * 0.5* SZ + float4(0, SZ, 0, 0), viewproj);
    v.texCoords = texRoot + float3(0.9, 0.1, 0) * 0.0625;
    OutputStream.Append(v);

}



float4 psMain(GSOut vOut) : SV_TARGET
{
    float4 samp = gAlbedo.Sample(gSampler, vOut.texCoords.xy);
    //samp.a = saturate(samp.a * 2);
    
    if (alpha_pass == 0) {
        clip(samp.a - 0.6);
        samp.a = 1;
    }
    else
    {
        clip(samp.a - 0.1); // all the trabnsparent stuff
        clip(0.6 - samp.a);
    }
    return samp;
}
