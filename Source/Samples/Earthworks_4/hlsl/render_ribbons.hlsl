
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		






SamplerState gSampler;
Texture2D gAlbedo : register(t0);


StructuredBuffer<ribbonVertex>        instanceBuffer;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eye;
};



struct VSOut
{
    float4 rootPos : ANCHOR;
    float4 right : TEXCOORD;
    uint flags : TEXCOORD1;
    float4 other : TEXCOORD2;
    float3 N : TEXCOORD3;
};

struct GSOut
{
    float4 pos : SV_POSITION;
    float3 texCoords : TEXCOORD;
    float3 N : TEXCOORD1;
    float3 world : TEXCOORD2;
};


int rand_5(in float2 uv)
{
    float2 noise = frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453);
    return noise.x * 50;
    //return floor(frac(abs(noise.x + noise.y)) * 5);
}



VSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    VSOut output = (VSOut)0;

    int x = iId & 0x7f;
    int y = iId >> 7;
    float3 Ipos = float3(x * 0.1, 0, y * 0.1);
    //int P = saturate(frac((float)iId * 0.2)) *5 * 128;
    int P = (iId & 0x3 )  * 128;

    float F = frac(iId / 50.f);
    P = floor(F * 50) * 128;


    ribbonVertex R = instanceBuffer[vId + P];

    output.rootPos = float4(Ipos + R.pos + float3(2984, 611.9, 5709), 1);
    output.right = float4(R.right * 0.004, 0);
    output.flags = R.A;
    output.other.x = R.pos.y * 4;

    float3 U = instanceBuffer[vId + P + 1].pos - R.pos;
    float3 E = R.pos - eye;
    E.z *= -1;
    float3 RGH = normalize(cross(U, E));
    //output.right = float4(RGH * 0.004, 0);

    output.N = normalize(cross(R.right, U));


    return output;
}



[maxvertexcount(4)]
void gsMain(line VSOut L[2], inout TriangleStream<GSOut> OutputStream)
{
    GSOut v;

    //OutputStream.RestartStrip();
    if (L[1].flags == 1) {


        v.pos = mul(L[0].rootPos - L[0].right, viewproj);
        v.texCoords = float3(0.1, 0.9, L[0].other.x);
        v.N = L[0].N;
        v.world = normalize(eye - L[0].rootPos.xyz);
        OutputStream.Append(v);

        v.pos = mul(L[0].rootPos + L[0].right, viewproj);
        v.texCoords = float3(0.9, 0.9, L[0].other.x);
        v.N = L[0].N;
        v.world = normalize(eye - L[0].rootPos.xyz);
        OutputStream.Append(v);


        v.pos = mul(L[1].rootPos - L[1].right, viewproj);
        v.texCoords = float3(0.1, 0.9, L[1].other.x);
        v.N = L[1].N;
        v.world = normalize(eye - L[1].rootPos.xyz);
        OutputStream.Append(v);

        v.pos = mul(L[1].rootPos + L[1].right, viewproj);
        v.texCoords = float3(0.9, 0.9, L[1].other.x);
        v.N = L[1].N;
        v.world = normalize(eye - L[1].rootPos.xyz);
        OutputStream.Append(v);
    }
    
}



float4 psMain(GSOut vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    float3 normal = vOut.N * (2* isFrontFace - 1);

    float3 sunDir = normalize(float3(0.7, -0.3, 0.2));
    float3 spec = reflect(sunDir, normal);
    float A = dot(normal, sunDir);
    float A_backface = saturate(-A);
    A = saturate(A + 0.1);
    float S = pow(abs(dot(vOut.world, spec)), 15);

    float3 colour = float3(0.07, 0.2, 0.05);
    float3 final = (colour * A * saturate(vOut.texCoords.z * 3))  +  float3(0.1, 0.2, 0) * A_backface  + S;
    return float4(final, 1);
}
