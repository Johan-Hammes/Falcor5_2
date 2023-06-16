
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		








SamplerState gSampler;
Texture2D gAlbedo : register(t0);

struct ribbonTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<ribbonTextures>
gribbonTextures;


StructuredBuffer<sprite_material>   materials;
StructuredBuffer<ribbonVertex>      instanceBuffer;
StructuredBuffer<xformed_PLANT>     instances;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eye;
};



struct VSOut
{
    float4 rootPos : ANCHOR;
    float4 right : TEXCOORD;
    uint4 flags : TEXCOORD1;
    float4 other : TEXCOORD2;
    float3 N : TEXCOORD3;
    float3 U : TEXCOORD4;
    float3 R : TEXCOORD5;
};

struct GSOut
{
    float4 pos : SV_POSITION;
    float3 texCoords : TEXCOORD;
    
    uint4 flags : TEXCOORD1;
    float3 world : TEXCOORD2;
    float3 N : TEXCOORD3;
    float3 U : TEXCOORD4;
    float3 R : TEXCOORD5;
    
};



VSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    VSOut output = (VSOut)0;

    //const xformed_PLANT plant = instances[iId];
    const uint P = 0;// plant.index * 128;
    const ribbonVertex R = instanceBuffer[vId + P];

    xformed_PLANT plant;;
    plant.position = float3(0, 0, 0);
    plant.rotation.x = 1;
    plant.rotation.y = 0;
    plant.scale = 1;

    output.rootPos = float4(plant.position + (float3(R.pos.x * plant.rotation.x + R.pos.z * plant.rotation.y, R.pos.y, R.pos.x * plant.rotation.y - R.pos.z * plant.rotation.x) * plant.scale ), 1);

    float3 up = float3(R.right.x * plant.rotation.x + R.right.z * plant.rotation.y, R.right.y, R.right.x * plant.rotation.y - R.right.z * plant.rotation.x) ;
    float3 dir = normalize(eye - output.rootPos.xyz);
    float3 right = normalize(cross(up, dir)) * length(up);
    output.right = float4(right, 0) * 0.5 * plant.scale;
    output.flags.x = R.A;
    output.flags.y = R.B;
    output.other.x = R.pos.y * 4;
    //float3 U = instanceBuffer[vId + P + 1].pos - R.pos; // FIXME needs to roatet
    output.N = normalize(cross(output.right.xyz, up));
    output.U = normalize(up);
    output.R = normalize(right);

    return output;
}



[maxvertexcount(4)]
void gsMain(line VSOut L[2], inout TriangleStream<GSOut> OutputStream)
{
    GSOut v;

    //OutputStream.RestartStrip();
    if (L[1].flags.x == 1)
    {
        v.texCoords = float3(0, 1.0, L[0].other.x);
        v.N = L[0].N;
        v.U = L[0].U;
        v.R = L[0].R;
        v.world = normalize(eye - L[0].rootPos.xyz);
        v.flags = L[0].flags;

        v.pos = mul(L[0].rootPos - L[0].right, viewproj);
        OutputStream.Append(v);

        v.pos = mul(L[0].rootPos + L[0].right, viewproj);
        v.texCoords.x = 1;
        OutputStream.Append(v);


        v.N = L[1].N;
        v.U = L[1].U;
        v.R = L[1].R;
        v.pos = mul(L[1].rootPos - L[1].right, viewproj);
        v.texCoords = float3(0, 0.0, L[1].other.x);
        v.world = normalize(eye - L[1].rootPos.xyz);
        OutputStream.Append(v);

        v.pos = mul(L[1].rootPos + L[1].right, viewproj);
        v.texCoords.x = 1;
        OutputStream.Append(v);
    }
    
}



float4 psMain(GSOut vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
        int material = vOut.flags.y;
    if (material == 2)
    {
        material = 1;
    }

        float4 albedoAlpha = gribbonTextures.T[material * 2 + 0].Sample(gSampler, vOut.texCoords.xy);
    clip(albedoAlpha.a - 0.7);

    float3 normalTex = ((gribbonTextures.T[material * 2 + 1].Sample(gSampler, vOut.texCoords.xy).rgb) * 2.0) - 1.0;

    float3 NORM = normalize((normalTex.r * vOut.R) + (normalTex.g * vOut.U) + (normalTex.b * vOut.N));
    //NORM = normalize(vOut.N );


    float3 eyeDir = normalize(eye - vOut.world);

    float alpha = 1;
    float3 normal = vOut.N * (2* isFrontFace - 1);

    //float3 sunDir = normalize(float3(0.7, -0.3, 0.2));
    float3 sunDir = normalize(float3(1.0, 0.2, 0.0));
    float3 spec = reflect(eyeDir, NORM);
    float A = dot(NORM, sunDir);
    float A_backface = saturate(-A);
    A = saturate(A);
    float S = pow(abs(dot(sunDir, spec)), 15);
    float AO = 1.0f;
    
    if (material < 0.5)
    {
        S *= 0.0;
        A *= 0.3;
    }
    else
    {
        A *= 0.8;
        S *= 0.3;
        //clip(-1);

    }

    if (vOut.flags.y == 1)
    {
        A *= 0.3;
        AO = 0.3;
        S *= 0.1;
    }
    
        float3 final = (albedoAlpha.rgb * A) + (albedoAlpha.rgb * float3(0.05, 0.1, 0.2)) * AO + S;

 
    return float4(final, albedoAlpha.a);
}
