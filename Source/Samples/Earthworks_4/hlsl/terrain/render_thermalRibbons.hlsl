#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"

SamplerState gSampler;
SamplerState gSamplerClamp;



StructuredBuffer<float4> vertexBuffer;



cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eyePos;
};




struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD; // texture uv
    float3 eye : TEXCOORD1; // can this become SVPR  or per plant somehow, feels overkill here per vertex
};



PSIn vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    PSIn output = (PSIn) 0;

    output.pos = vertexBuffer[iId * 3 + vId];
    output.pos.w = 1;
    output.uv.x = vId;
    output.uv.y = vertexBuffer[iId * 3 + vId].w;
    output.eye = normalize(output.pos.xyz - eyePos);
    
    return output;
}

[maxvertexcount(4)]
void gsMain(line PSIn L[2], inout TriangleStream<PSIn> OutputStream)
{
    PSIn v;
    float4 tangent = float4(5, 0, 0, 0);

    if (L[0].uv.x < 2.5)
    {
    
        v = L[0];
        v.pos = mul(L[0].pos + tangent, viewproj);
        OutputStream.Append(v);

        v.pos = mul(L[0].pos - tangent, viewproj);
        OutputStream.Append(v);

        
        v = L[1];
        v.pos = mul(L[1].pos + tangent, viewproj);
        OutputStream.Append(v);

        v.pos = mul(L[1].pos - tangent, viewproj);
        OutputStream.Append(v);
    }
}



float4 psMain(PSIn vOut) : SV_TARGET
{
    if (vOut.uv.y < 0.5)
        return float4(0, 0, 1, 1);
    if (vOut.uv.y < 1.0)
        return float4(0, 1, 0, 1);
    if (vOut.uv.y < 2.0)
        return float4(1, 1, 0, 1);
    
    return float4(1, 0, 0, 1);
}
