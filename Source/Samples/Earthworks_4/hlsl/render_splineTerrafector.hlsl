#include "render_Common.hlsli"
#include "materials.hlsli"


SamplerState  SMP;
Texture2D textures[48];		//    switch: /enable_unbounded_descriptor_tables    D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES
StructuredBuffer<TF_material> materials;


cbuffer gConstantBuffer
{
    float4x4 view;
	float4x4 proj;
	float4x4 viewproj;
	
	int bHidden;
	int numSubdiv;
	float dLeft;
	float dRight;
	
	float4 colour;
};


struct splineVSOut
{
    float4 pos : SV_POSITION;
	float3 posW : POSITION;
    float2 texCoords : TEXCOORD;
	nointerpolation uint4 flags : TEXCOORD1;
	float4 height : TEXCOORD2;
	float4 colour : COLOR;
};


struct cubicDouble
{
	float4 data[2][4];			//[middle, outside][p0, p1, p2, p3]
};


struct bezierLayer
{
	uint A;						// flags, material, index [3][13][16]			combine with rootindex in constant buffer
	uint B;						// w0, w1 [16][16]
};


StructuredBuffer<cubicDouble> splineData;
StructuredBuffer<bezierLayer> indexData;


// Cateljau
inline float4 quadratic_Casteljau(float t, float4 P0, float4 P1, float4 P2)
{
	float4 pA = lerp(P0, P1, t);
	float4 pB = lerp(P1, P2, t);
	
	return lerp(pA, pB, t);
}


inline float4 cubic_Casteljau(float t, float4 P0, float4 P1, float4 P2, float4 P3)
{
	float4 pA = lerp(P0, P1, t);
	float4 pB = lerp(P1, P2, t);
	float4 pC = lerp(P2, P3, t);
	
	float4 pD  = lerp(pA, pB, t);
	float4 pE  = lerp(pB, pC, t);
	
	return lerp(pD, pE, t);   
}


#define outsideLine 	(segment.A >> 31 ) & 0x1
#define insideLine 		(segment.A >> 30 ) & 0x1

#define isStartOverlap 	(segment.B >> 31 ) & 0x1
#define isEndOverlap 	(segment.B >> 30 ) & 0x1
#define isQuad 			(segment.B >> 29 ) & 0x1
#define isOverlap		(isStartOverlap) || (isEndOverlap)

// 12 bits left 4096 materials
#define material  		(segment.A >> 17) & 0x7ff		

#define index  			segment.A & 0x1ffff



splineVSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    splineVSOut output;
	
	
	float3 pos, tangent;


	// unpack --------------------------------------------------------------------------
	// FIXME, proper unpack function
	const bezierLayer segment = indexData[ iId ];
	
	
	const float w0 = ((segment.B >> 14) & 0x3fff) * 0.002f - 16.0f;			// -32 .. 33.536m in mm resolution
	const float w1 = (segment.B & 0x3fff) * 0.002f - 16.0f;
	
	const cubicDouble s0 = splineData[index];
	
	float4 points[2];
	const uint bezierIndex = vId>>1;
	float t = bezierIndex /(float)numSubdiv;
	
	float overlapAlpha = 1.0f;
	if (isOverlap) overlapAlpha = 0.0f;
	
	if (isQuad)
	{
		if (bezierIndex == 0)
		{
			points[0] = s0.data[0][0];
			points[1] = s0.data[1][0];
		}
		else
		{
			t = 1.0f;	// to fix UV's for quads
			points[0] = s0.data[0][1];
			points[1] = s0.data[1][1];
		}
	}
	else
	{
		points[0] = cubic_Casteljau(t, s0.data[0][0], s0.data[0][1], s0.data[0][2], s0.data[0][3]);
		points[1] = cubic_Casteljau(t, s0.data[1][0], s0.data[1][1], s0.data[1][2], s0.data[1][3]);
		
		if (isStartOverlap)
		{
				overlapAlpha = saturate(1 - (bezierIndex * 0.2));
		}
		
		if (isEndOverlap && bezierIndex > (numSubdiv>>1))
		{
				overlapAlpha = saturate(1 - ((numSubdiv - bezierIndex) * 0.2));
		}
	}
	
	float3 perpendicular = points[1].xyz - points[0].xyz;
	const float width = length(perpendicular);
	perpendicular = normalize(perpendicular);

	float3 position = float3(0,0,0);;
	
	if (vId & 0x1 ) {			// outside
		position = points[outsideLine].xyz + w1 * perpendicular;
	} else {					// inside
		position = points[insideLine].xyz + w0 * perpendicular;
	}
	
	
	
	output.texCoords = float2(vId & 0x1, points[outsideLine].w );
	output.pos =  mul( float4(position, 1), viewproj);
	output.posW = position;
	output.height.x = position.y;
	//output.flags.x = bHidden;
	//output.flags.y = (segment.A >> 16) & 0xfff;
	output.flags.x = ((segment.A >> 16) & 0xf);
	//output.flags.x = 2;
	//if (((segment.A >> 16) & 0xfff) == 2047) output.flags.x = 1;
	output.colour = colour;
	output.colour.r = output.texCoords.x;
	output.colour.a = overlapAlpha;
	
	
	output.flags.x = 0;
	uint MAT = ((indexData[ iId ].A >> 16) & 0xfff);
	output.flags.x = MAT;
	
	return output;
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


PS_OUTPUT psMain(splineVSOut vIn)  : SV_TARGET
{
	PS_OUTPUT output;// = (PS_OUTPUT)0;

	output.Elevation = 0;
	output.Albedo = 0;
	output.PBR = 0;
	output.Alpha = 0;
	output.Ecotope1 = 0;
	output.Ecotope2 = 0;
	output.Ecotope3 = 0;
	output.Ecotope4 = 0;

	uint material = vIn.flags.x;
	
	

	// couple of fixed materials
	// 2048 materials MAX
	
	if(material == 4093) 				// vertex colour blend - verge etc
	{
		output.Elevation.a = smoothstep(0, 1, vIn.colour.r);
		output.Elevation.r = vIn.posW.y * output.Elevation.a;
		return output;
	}
	
	
	if(material == 4094)  				// solid
	{
		output.Elevation.a = 1;
		output.Elevation.r = vIn.posW.y;
		return output;
	}
	
	if(material == 4095)				// curvature
	{
		//output.Elevation.a = 0;
		//output.Elevation.r = 0.1 * cos((1 - vIn.texCoords.x) * 1.57079632679);
		return output;
	}
	

	
	
	
	// get the materials
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
		float alphaBase = textures[MAT.baseAlphaTexture].Sample(SMP, alpha_uv).r;
		if (MAT.baseAlphaClampU && alpha_uv.x > 0.95)
		{
			alphaBase = 1;
		}
		float alphaDetail = textures[MAT.detailAlphaTexture].Sample(SMP, uvWorld).r;
		
		alpha = lerp(1,  smoothstep(0, 1, vIn.colour.r), MAT.vertexAlphaScale);
		alpha *=  lerp(1, saturate((alphaBase + MAT.baseAlphaBrightness) * MAT.baseAlphaContrast ), MAT.baseAlphaScale);
		alpha *=  lerp(1, saturate((alphaDetail + MAT.detailAlphaBrightness) * MAT.detailAlphaContrast  ), MAT.detailAlphaScale);
	}
	
	
	// Elevation
	output.Elevation = 0;
	if (MAT.useElevation)
	{
		if(MAT.useVertexY) 
		{
			output.Elevation.r = vIn.posW.y;
		}
		output.Elevation.r += MAT.YOffset;
		output.Elevation.r += (textures[MAT.baseElevationTexture].Sample(SMP, uv).r - MAT.baseElevationOffset) * MAT.baseElevationScale;
		output.Elevation.r += (textures[MAT.detailElevationTexture].Sample(SMP, uvWorld).r - MAT.detailElevationOffset) * MAT.detailElevationScale;
		
		output.Elevation.r *= alpha;
		
		if(MAT.useAbsoluteElevation > 0.5) {
			output.Elevation.a = alpha;  
		}
		else {
			output.Elevation.a = 0;  // since that causes OneMinusSrcAlpha to 1
		}
		
	}
	
	
	
	
	
	if(MAT.useColour)
	{
		float3 albedo = textures[MAT.baseAlbedoTexture].Sample(SMP, uv).rgb;
		float3 albedoDetail = textures[MAT.detailAlbedoTexture].Sample(SMP, uvWorld).rgb;
		
		float3 A = lerp(albedo, 0.5, saturate(MAT.albedoBlend));
		float3 B = lerp(albedoDetail, 0.5, saturate(-MAT.albedoBlend));
		
		output.Albedo.rgb = clamp(0.04, 0.9,  A * B * 4 * MAT.albedoScale );		// 0.04, 0.9 charcoal to fresh snow
		output.Albedo.a = alpha;
		
		
		float baseRoughness = textures[MAT.baseRoughnessTexture].Sample(SMP, uv).r;
		float detailRoughness = textures[MAT.detailRoughnessTexture].Sample(SMP, uvWorld).r;
		
		float rA = lerp(baseRoughness, 0.5, saturate(MAT.roughnessBlend));
		float rB = lerp(detailRoughness, 0.5, saturate(-MAT.roughnessBlend));
		
		output.PBR.rgb = saturate( pow(rA * rB * 2, MAT.roughnessScale) );
		output.PBR.a = alpha;
	}
	

	
	output.Alpha=float4(1, 1, 1, 1);

	output.Ecotope1=float4(0, 0, 0, alpha);
	output.Ecotope2=float4(0, 0, 0, alpha);
	output.Ecotope3=float4(0, 0, 0, alpha);
	output.Ecotope4=float4(0, 0, 0, alpha);
	
	return output;
}
	

