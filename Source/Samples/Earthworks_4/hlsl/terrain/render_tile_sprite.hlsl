
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		
#include "vegetation_defines.hlsli"
#include "../render_Common.hlsli"



SamplerState gSampler;
//SamplerState gSmpLinearClamp;

TextureCube gEnv : register(t1);
Texture2D gHalfBuffer : register(t2);

struct ribbonTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<ribbonTextures>  textures;



StructuredBuffer<instance_PLANT>        instanceBuffer;
RWStructuredBuffer<gpuTile> 			tiles;
StructuredBuffer<tileLookupStruct>		tileLookup;

StructuredBuffer<sprite_material> materials;
StructuredBuffer<plant> plant_buffer;



cbuffer gConstantBuffer
{
    float4x4 viewproj;
    
	float3 	right;
	int		alpha_pass;
    
    float3 sunDirection_OLD;
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


/*
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
*/
struct GSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float scale : TEXCOORD1;
    float3 sunlight : TEXCOORD2;
    float3 inscatter : TEXCOORD3;
    float3 outscatter : TEXCOORD4;
    float3 worldPos : TEXCOORD5;
    float3 eye : TEXCOORD6;
    uint index : TEXCOORD7;
};


GSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    GSOut output = (GSOut) 0;

    
    uint tileIDX = tileLookup[iId].tile & 0xffff;
    uint numQuad = tileLookup[iId].tile >> 16;
    uint blockID = vId;// (iId & 0x3f);

    // FIXME just fill the rest of the 64 with NULL and render all much faster than this if
    if (blockID < numQuad)
    {
        uint newID = tileLookup[iId].offset + blockID;
        instance_PLANT plant = instanceBuffer[newID];

        output.pos = float4(unpack_pos(plant.xyz, tiles[tileIDX].origin, tiles[tileIDX].scale_1024), 1);
        output.scale = 2;//        SCALE(plant.s_r_idx) * 0.4;
        output.index = PLANT_INDEX(plant.s_r_idx) * 4;
        output.sunlight = sunLight(output.pos.xyz * 0.001).rgb;

        // Now for my atmospeher code --------------------------------------------------------------------------------------------------------

        float3 atmosphereUV;
        float4 pos = mul(output.pos, viewproj);
        atmosphereUV.xy = pos.xy / pos.w / screenSize;
        atmosphereUV.z = log(length(output.pos.xyz - eye.xyz) / fog_far_Start) * fog_far_log_F + fog_far_one_over_k;
        atmosphereUV.z = max(0.01, atmosphereUV.z);
        atmosphereUV.z = min(0.99, atmosphereUV.z);
        output.outscatter = gAtmosphereOutscatter.SampleLevel(gSmpLinearClamp, atmosphereUV, 0 ).rgb;
        output.inscatter = gAtmosphereInscatter.SampleLevel(gSmpLinearClamp, atmosphereUV, 0).rgb;


        output.worldPos = output.pos.xyz;
        output.eye = output.pos.xyz - eye.xyz;
        output.uv = float2(0, 0);

       //output.rootPos.xyz -= normalize(output.eye) * 100.f;
        //output.rootPos.y += 300;
        //output.rootPos.x += 7000;

    }
    return output;
}



[maxvertexcount(4)]
void gsMain(point GSOut sprite[1], inout TriangleStream<GSOut> OutputStream)
{
    GSOut v = sprite[0];


    const plant PLANT = plant_buffer[sprite[0].index];
    //const plant PLANT = plant_buffer[4];

    uint I = sprite[0].index;
    int dx = I & 0xf;
    int dy = I >> 4;
    //float SZ = plant.size.x; //    sprite[0].scale * size[I];
    
    
    //create sprite quad
    //--------------------------------------------
    float3 texRoot = float3(dx, dy, 0) * 0.0625;

    v.pos = mul(sprite[0].pos, viewproj);
    
    float X = abs(v.pos.x / v.pos.z);
    float Y = abs(v.pos.y / v.pos.z);
    if (v.pos.z < 1 || X > 1.3 || Y > 2.1)
    {
    
        //OutputStream.Append(v);
    }
    else
    {
        // add back in       float scale = 1 - pt[0].lineScale.z;
        float X = PLANT.size.x * 0.8;
        float Y = PLANT.size.y;
        float scale = 1 - 0.8;

        v.uv = float2(0.5, 1.1);
        v.pos = mul(sprite[0].pos + float4(0, -Y * 0.1, 0, 0), viewproj);
        OutputStream.Append(v);

        v.uv = float2(1.0 - scale / 2, 0.5);
        v.pos = mul(sprite[0].pos - float4(right, 0)  * X + float4(0, Y * 0.5, 0, 0), viewproj);
        OutputStream.Append(v);
        
        v.uv = float2(0.0 + scale / 2, 0.5);
        v.pos = mul(sprite[0].pos + float4(right, 0)  * X + float4(0, Y * 0.5, 0, 0), viewproj);
        OutputStream.Append(v);

        v.uv = float2(0.5, -0.1);
        v.pos = mul(sprite[0].pos + float4(0, Y * 1.1, 0, 0), viewproj);
        OutputStream.Append(v);
    }
 
}



float4 psMain(GSOut vOut) : SV_TARGET
{
    clip(vOut.uv.y);

    const plant PLANT = plant_buffer[vOut.index];
    const sprite_material MAT = materials[PLANT.billboardMaterialIndex];

    float4 samp = 1;

    float4 albedo = textures.T[MAT.albedoTexture].Sample(gSmpLinearClamp, vOut.uv.xy);
    albedo.rgb *= MAT.albedoScale[0] * 2.f;
    float alpha = pow(albedo.a, MAT.alphaPow);
    clip(alpha - 0.5);
    alpha = smoothstep(0.5, 0.8, alpha);
/*
    float3 N = vOut.normal;
    if (MAT.normalTexture >= 0)
    {
        float3 normalTex = ((textures.T[MAT.normalTexture].Sample(gSamplerClamp, vOut.uv.xy).rgb) * 2.0) - 1.0;
        N = (normalTex.r * vOut.tangent) + (normalTex.g * vOut.binormal) + (normalTex.b * vOut.normal);
    }
    N *= flipNormal;
    float ndoth = saturate(dot(N, normalize(sunDirection + vOut.eye)));
    float ndots = dot(N, sunDirection);
*/
    samp = albedo;
    return samp;
}
