
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
SamplerState gSmpLinearClamp;

Texture2D gAlbedo : register(t0);
Texture2D gNorm : register(t1);
//Texture2DArray gNorm : register(t1);
//Texture2DArray gTranclucent : register(t2);
//TextureCube gCube;

Texture3D gAtmosphereInscatter;
Texture3D gAtmosphereOutscatter;
Texture2D SunInAtmosphere;

StructuredBuffer<instance_PLANT>        instanceBuffer;
RWStructuredBuffer<gpuTile> 			tiles;
StructuredBuffer<tileLookupStruct>		tileLookup;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
    
	float3 	right;
	int		alpha_pass;
    
    float3 sunDirection;
    float padd001;

    float2 screenSize;
    float fog_far_Start;
    float fog_far_log_F; //(k-1 / k) / log(far)		// FIXME might be k-2 to make up for half pixel offsets
	
    float fog_far_one_over_k; // 1.0 / k
    float fog_near_Start;
    float fog_near_log_F; //(k-1 / k) / log(far)		// FIXME might be k-2 to make up for half pixel offsets
    float fog_near_one_over_k; // 1.0 / k

    float3 eye;
};


float4 sunLight(float3 posKm)
{
    float2 sunUV;
    float dX = dot(posKm.xz, normalize(sunDirection.xz));
    sunUV.x = saturate(0.5 - (dX / 1600.0f));
    sunUV.y = 1 - saturate(posKm.y / 100.0f);
    return SunInAtmosphere.SampleLevel(gSmpLinearClamp, sunUV, 0) * 0.07957747154594766788444188168626;
}


struct VSOut
{
    float4 rootPos : ANCHOR;
    float scale : TEXCOORD;
    uint index : TEXCOORD1;
    float3 sunlight : TEXCOORD2;
    float3 inscatter : TEXCOORD3;
    float3 outscatter : TEXCOORD4;
    float3 worldPos : TEXCOORD5;
    float3 eye : TEXCOORD6;
};

struct GSOut
{
    float4 pos : SV_POSITION;
    float3 texCoords : TEXCOORD;
    float3 sunlight : TEXCOORD2;
    float3 inscatter : TEXCOORD3;
    float3 outscatter : TEXCOORD4;
    float3 worldPos : TEXCOORD5;
    float3 eye : TEXCOORD6;
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
        output.scale = SCALE(plant.s_r_idx) * 0.4;
        output.index = PLANT_INDEX(plant.s_r_idx);
        output.sunlight = sunLight(output.rootPos.xyz * 0.001).rgb;

        // Now for my atmospeher code --------------------------------------------------------------------------------------------------------
        float3 atmosphereUV;
        float4 pos = mul(output.rootPos, viewproj);
        atmosphereUV.xy = pos.xy / pos.w / screenSize;
        atmosphereUV.z = log(length(output.rootPos.xyz - eye.xyz) / fog_far_Start) * fog_far_log_F + fog_far_one_over_k;
        atmosphereUV.z = max(0.01, atmosphereUV.z);
        atmosphereUV.z = min(0.99, atmosphereUV.z);
        output.outscatter = gAtmosphereOutscatter.SampleLevel(gSmpLinearClamp, atmosphereUV, 0 ).rgb;
        output.inscatter = gAtmosphereInscatter.SampleLevel(gSmpLinearClamp, atmosphereUV, 0).rgb;

        output.worldPos = output.rootPos.xyz;
        output.eye = output.rootPos.xyz - eye.xyz;
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

    v.sunlight = sprite[0].sunlight;
    v.inscatter = sprite[0].inscatter;
    v.outscatter = sprite[0].outscatter;
    v.worldPos = sprite[0].worldPos;
    v.eye = sprite[0].eye;

    
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
    float3 norm = gNorm.Sample(gSampler, vOut.texCoords.xy).rgb;
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
    samp.rgb *=  0.02;//    saturate(dot(norm, -sunDirection)) * vOut.sunlight;
    //samp.rgb *= vOut.outscatter;
    //samp.rgb += vOut.inscatter;
	
    {
    
        float3 atmosphereUV;
        atmosphereUV.xy = vOut.pos.xy / screenSize;
        atmosphereUV.z = log(length(vOut.eye.xyz) / fog_far_Start) * fog_far_log_F + fog_far_one_over_k;
        atmosphereUV.z = max(0.01, atmosphereUV.z);
        atmosphereUV.z = min(0.99, atmosphereUV.z);
        samp.rgb *= gAtmosphereOutscatter.Sample(gSmpLinearClamp, atmosphereUV).rgb;
        samp.rgb += gAtmosphereInscatter.Sample(gSmpLinearClamp, atmosphereUV).rgb;
    }
   
    
    return samp;
}
