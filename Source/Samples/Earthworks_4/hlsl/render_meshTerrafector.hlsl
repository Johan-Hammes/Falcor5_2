//#include "terrainCommon.hlsli"
#include "render_Common.hlsli"
#include "materials.hlsli"


struct  triVertex {
    float3      pos;
    float       alpha;      // might have to be full float4 colour   but look at float16

    float2      uv;         // might have to add second UV
    uint        material;
    float       buffer;         // now 32
};


//SamplerState  SMP : register(s0);
StructuredBuffer<TF_material> materials;
StructuredBuffer<triVertex> vertexData;
StructuredBuffer<uint> indexData;


struct myTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<myTextures> gmyTextures;





cbuffer gConstantBuffer : register(b0)
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
};



struct splineVSOut
{
    float4 pos : SV_POSITION;
    float3 posW : POSITION;
    float3 texCoords : TEXCOORD;
    nointerpolation uint4 flags : TEXCOORD1;
    float4 colour : COLOR;
};



splineVSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    splineVSOut vsOut;

    uint idx = (iId * 128 * 3) + vId;
    //uint idx = vId;
    triVertex V = vertexData[indexData[idx]];
    //triVertex V = vertexData[vId];

    vsOut.pos = mul(float4(V.pos, 1), viewproj);
    vsOut.posW = V.pos;
    vsOut.flags.y = V.material;
    vsOut.colour = V.alpha;
    vsOut.texCoords = float3(V.uv, 0);

    return vsOut;
}





struct PS_OUTPUT
{
    float4 Elevation: SV_Target0;
    float4 Albedo: SV_Target1;
	float4 PBR: SV_Target2;
	float4 Alpha: SV_Target3;
	float4 Ecotope1: SV_Target4;
	float4 Ecotope2: SV_Target5;
	float4 Ecotope3: SV_Target6;
	float4 Ecotope4: SV_Target7;
};


PS_OUTPUT psMain(splineVSOut vIn) : SV_TARGET
{
    
	PS_OUTPUT output = (PS_OUTPUT)0;

    // get the materials
    uint material = vIn.flags.y;
    TF_material MAT = materials[material];

    // uv's
    float2 uv = vIn.texCoords.xy * MAT.uvScale;
    float2 uvWorld = vIn.posW.xz / MAT.worldSize;


    // alpha
    float alpha = 1;
    if (MAT.useAlpha)
    {
        float2 alpha_uv = uv;
        if (MAT.baseAlphaClampU)
        {
            alpha_uv = vIn.texCoords.xy * MAT.uvScaleClampAlpha;
            alpha_uv.x = clamp(alpha_uv.x, 0.05, 1.0);
        }
        float alphaBase = gmyTextures.T[MAT.baseAlphaTexture].Sample(gSmpLinear, alpha_uv).r;
        if (MAT.baseAlphaClampU && alpha_uv.x > 0.95)
        {
            alphaBase = 1;
        }
        float alphaDetail = gmyTextures.T[MAT.detailAlphaTexture].Sample(gSmpLinear, uvWorld).r;

        alpha = lerp(1, smoothstep(0, 1, vIn.colour.r), MAT.vertexAlphaScale);
        alpha *= lerp(1, saturate((alphaBase + MAT.baseAlphaBrightness) * MAT.baseAlphaContrast), MAT.baseAlphaScale);
        alpha *= lerp(1, saturate((alphaDetail + MAT.detailAlphaBrightness) * MAT.detailAlphaContrast), MAT.detailAlphaScale);
    }


    // Elevation
    output.Elevation = 0;
    if (MAT.useElevation)
    {
        if (MAT.useVertexY)
        {
            output.Elevation.r = vIn.posW.y;
        }
        output.Elevation.r += MAT.YOffset;
        output.Elevation.r += (gmyTextures.T[MAT.baseElevationTexture].Sample(gSmpLinear, uv).r - MAT.baseElevationOffset) * MAT.baseElevationScale;
        output.Elevation.r += (gmyTextures.T[MAT.detailElevationTexture].Sample(gSmpLinear, uvWorld).r - MAT.detailElevationOffset) * MAT.detailElevationScale;

        output.Elevation.r *= alpha;

        if (MAT.useAbsoluteElevation > 0.5) {
            output.Elevation.a = alpha;
        }
        else {
            output.Elevation.a = 0;  // since that causes OneMinusSrcAlpha to 1
        }

    }





    if (MAT.useColour)
    {
        float3 albedo = gmyTextures.T[MAT.baseAlbedoTexture].Sample(gSmpLinear, uv).rgb;
        float3 albedoDetail = gmyTextures.T[MAT.detailAlbedoTexture].Sample(gSmpLinear, uvWorld).rgb;

        float3 A = lerp(albedo, 0.5, saturate(MAT.albedoBlend));
        float3 B = lerp(albedoDetail, 0.5, saturate(-MAT.albedoBlend));

        output.Albedo.rgb = clamp(0.04, 0.9, A * B * 4 * MAT.albedoScale);		// 0.04, 0.9 charcoal to fresh snow
        output.Albedo.a = alpha;


        float baseRoughness = gmyTextures.T[MAT.baseRoughnessTexture].Sample(gSmpLinear, uv).r;
        float detailRoughness = gmyTextures.T[MAT.detailRoughnessTexture].Sample(gSmpLinear, uvWorld).r;

        float rA = lerp(baseRoughness, 0.5, saturate(MAT.roughnessBlend));
        float rB = lerp(detailRoughness, 0.5, saturate(-MAT.roughnessBlend));

        output.PBR.rgb = saturate(pow(rA * rB * 2, MAT.roughnessScale));
        output.PBR.a = alpha;
    }


    output.Alpha = float4(1, 1, 1, 1);

    output.Ecotope1 = float4(0, 0, 0, alpha);
    output.Ecotope2 = float4(0, 0, 0, alpha);
    output.Ecotope3 = float4(0, 0, 0, alpha);
    output.Ecotope4 = float4(0, 0, 0, alpha);

    return output;
}

