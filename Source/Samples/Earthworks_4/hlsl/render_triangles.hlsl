
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		






SamplerState gSampler;
Texture2D gAlbedo : register(t0);

TextureCube gSky : register(t1);


StructuredBuffer<triangleVertex>        instanceBuffer;
StructuredBuffer<xformed_PLANT>       instances;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eye;
};



struct VSOut
{
    float4 pos : SV_POSITION;
    float2 texCoords : TEXCOORD;
    float3 N : TEXCOORD1;
    float3 world : TEXCOORD2;
};



VSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    VSOut output = (VSOut)0;

    const xformed_PLANT plant = instances[iId];
    const uint P = 0;// plant.index * 128;
    const triangleVertex R = instanceBuffer[vId + P];

    //float3 vPos = float3(R.pos.x * plant.rotation.x + R.pos.z * plant.rotation.y, R.pos.y, R.pos.x * plant.rotation.y - R.pos.z * plant.rotation.x);
    //output.pos = mul( float4(plant.position + vPos * plant.scale*40.4, 1), viewproj );
    float3 vPos = R.pos * 1000 + eye;
    output.pos = mul(float4(vPos, 1), viewproj);
    output.texCoords = float2(R.u, R.v);
    output.N = R.norm;
    //output.world = normalize(eye - plant.position);
    output.world = R.pos;

    return output;
}





float4 psMain(VSOut vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    float3 dir = normalize(vOut.world.xyz);
    dir.z *= -1;
    return gSky.SampleLevel(gSampler, dir, 0);
    //return float4(normalize(vOut.world.xyz) * 0.5 + 0.5, 1);
    
    float3 normal = vOut.N * (2* isFrontFace - 1);

    float3 sunDir = normalize(float3(0.7, -0.3, 0.2));
    float3 spec = reflect(sunDir, normal);
    float A = dot(normal, sunDir);
    float A_backface = saturate(-A);
    A = saturate(A + 0.1);
    float S = pow(abs(dot(vOut.world, spec)), 25);

    float3 colour = float3(0.07, 0.1, 0.05) * 0.5;
    float3 final = colour + float3(0.1, 0.1, 0) * A_backface  + S * 0.2;
    return float4(final, 1);
}
