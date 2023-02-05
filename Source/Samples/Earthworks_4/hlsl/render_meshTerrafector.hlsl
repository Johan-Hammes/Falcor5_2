#include "terrainCommon.hlsli"




MainVsOut vsMain(VS_IN vIn)
{
    MainVsOut vsOut;
    vsOut.vsData = defaultVS(vIn);
    vsOut.shadowsDepthC = mul(float4(vsOut.vsData.posW, 1) , camVpAtLastCsmUpdate).z;
    return vsOut;
}









__import ShaderCommon;
__import Shading;
__import DefaultVS;
__import Effects.CascadedShadowMap;
#include "terrainCommon.hlsli"


Texture2D<float> gTexDisplacement;
Texture2D<float> gTexDisplacementDetail;
Texture2D gTexAlbedo;
Texture2D gTexAlbedoDetail;
Texture2D gTexPBR;
Texture2D gTexPBRDetail;
Texture2D gTexEcotope;
Texture2D gTexAlpha;	// alpha




cbuffer gTopdownConstants
{
	int heightVertex;	// 0 or 1 really
	float heightScale;
	float heightDetailScale;
	float detailTextureSize;
	
	float uvScale0;
	float uvScale1;
	int rubbering;
	float heightBias;
	
	float heightDetailBias;
	float useAlphaVertex;
	float useAlphaTexture;
	float useAlphaWorldTexture;
	
	float objAlphaScale;
	float objAlphaOffset;
	float worldAlphaScale;
	float worldAlphaOffset;

	float heightPermanence;
	float colourPermanence;
	float ecotopePermanence;
	float detailColourBlend;	
	
	float scaleAlbedo;
	float scaleFresnel;
	float scaleRougness;
	float scaleAO;

	float4x4 ecotopeTextureMap[4];
};



SamplerState  SMP;


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


PS_OUTPUT psMain(MainVsOut vOut) : SV_TARGET
{
	PS_OUTPUT output = (PS_OUTPUT)0;
	
	
	// UV's
	float2 UV_0 = vOut.vsData.texC * uvScale0;
	float2 UV_1 = vOut.vsData.texC * uvScale1;									// FIXME BUG - must be second UV
	float2 UV_world = vOut.vsData.posW.xz / detailTextureSize;				  	// some degree of bias could help resolve sampling errors far from the origin
	
	
	// Alpha
	const float vertexAlpha = vOut.vsData.colorV.r;
	float alpha =  lerp(1, smoothstep(0, 1, vertexAlpha), useAlphaVertex);
	
	if (useAlphaTexture > 0) {
		const float objAlpha = saturate( gTexAlpha.Sample(SMP, UV_0).r * objAlphaScale + objAlphaOffset );
		alpha *= lerp(1, objAlpha, useAlphaTexture);
	}
	
	if (useAlphaWorldTexture > 0) {
		const float worldAlpha = saturate( gTexAlpha.Sample(SMP, UV_world).r * worldAlphaScale + worldAlphaOffset  );
		alpha *= lerp(1, worldAlpha, useAlphaWorldTexture);
	}
	
	
	// Elevation
	output.Elevation.r = heightVertex * vOut.vsData.posW.y;
	//output.Elevation.r += heightScale * (gTexDisplacement.Sample(SMP, vOut.vsData.texC) - heightBias);
	//output.Elevation.r += heightDetailScale * (gTexDisplacementDetail.Sample(SMP, UV_world) - heightDetailBias);
	output.Elevation.a = alpha;  	

	output.Elevation.r = heightVertex * vOut.vsData.posW.y;	
	output.Elevation.r += heightScale * (gTexDisplacement.Sample(SMP, vOut.vsData.texC) - heightBias);
	output.Elevation.r += heightDetailScale * (gTexDisplacementDetail.Sample(SMP, UV_world) - heightDetailBias);
	output.Elevation.a = alpha;  
		
	// albedo and pbr
	// FIXME ADD Detail texure ??? PBR blend
	float3 A = lerp(gTexAlbedo.Sample(SMP, vOut.vsData.texC).rgb, 0.5, saturate(detailColourBlend));
	float3 B = lerp(gTexAlbedoDetail.Sample(SMP, UV_world).rgb, 0.5, saturate(-detailColourBlend));
    output.Albedo.rgb = clamp(0.04, 0.95,  A*B*scaleAlbedo*2 );		// 0.04, 0.95 charcoal to fresh snow
	output.Albedo.a = alpha;
	
	
	output.Elevation.a = 0;
	output.Albedo = float4(0.0, 0.03, 0, 0.7);
	
	A = lerp(gTexPBR.Sample(SMP, vOut.vsData.texC).rgb, 0.5, saturate(detailColourBlend));
	B = lerp(gTexPBRDetail.Sample(SMP, UV_world).rgb, 0.5, saturate(-detailColourBlend));
	output.PBR.rgb = saturate( A*B*float3(scaleFresnel, scaleRougness, scaleAO)*2 );
	output.PBR.a = alpha;
	
	// alpha
	output.Alpha=float4(heightPermanence * alpha, colourPermanence * alpha, ecotopePermanence * alpha, 1);
	
	// ecotopes
	// FIXME MUST GO to 1.0 is not in use this is a negative setup - multiplies down to zero to remove ??? or is it an inverse setup ??? TEST
	float4 ect = gTexEcotope.Sample(SMP, vOut.vsData.texC);
	output.Ecotope1=mul(ect, ecotopeTextureMap[0]);
	output.Ecotope2=mul(ect, ecotopeTextureMap[1]);
	output.Ecotope3=mul(ect, ecotopeTextureMap[2]);
	output.Ecotope4=mul(ect, ecotopeTextureMap[3]);
	
	
	return output;
}

