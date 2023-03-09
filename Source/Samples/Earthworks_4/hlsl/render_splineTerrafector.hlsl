
#include "materials.hlsli"


SamplerState  SMP : register(s0);
StructuredBuffer<TF_material> materials;
StructuredBuffer<cubicDouble> splineData;
StructuredBuffer<bezierLayer> indexData;

struct myTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<myTextures> gmyTextures;

cbuffer gConstantBuffer : register(b0)
{
    float4x4 viewproj;
    uint startOffset;
};


struct splineVSOut
{
    float4 pos : SV_POSITION;
	float3 posW : POSITION;
    float3 texCoords : TEXCOORD;
	uint4 flags : TEXCOORD1;
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


#define outsideLine 	(indexData[ iId + startOffset ].A >> 31) & 0x1
#define insideLine 		(indexData[ iId + startOffset ].A >> 30) & 0x1
#define materialFlag  	(indexData[ iId + startOffset ].A >> 17) & 0x7ff		    // 11 bits left 2048 materials
#define index  			indexData[ iId + startOffset ].A & 0x1ffff

#define isStartOverlap 	(indexData[ iId + startOffset ].B >> 31) & 0x1
#define isEndOverlap 	(indexData[ iId + startOffset ].B >> 30) & 0x1
#define isQuad 			(indexData[ iId + startOffset ].B >> 29) & 0x1
#define isFlipped 		(indexData[ iId + startOffset ].B >> 28) & 0x1
#define isOverlap		(isStartOverlap) || (isEndOverlap)



splineVSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    splineVSOut output;
    output.posW = 0;

    float4 points[2];
    float t = (vId >> 1) / 64.0;
    const cubicDouble s0 = splineData[index];
    points[0] = cubic_Casteljau(t, s0.data[0][0], s0.data[0][1], s0.data[0][2], s0.data[0][3]);
    points[1] = cubic_Casteljau(t, s0.data[1][0], s0.data[1][1], s0.data[1][2], s0.data[1][3]);

    float3 perpendicular = normalize(points[1].xyz - points[0].xyz);

    float overlapAlpha = 1.0f;
    if (isOverlap) overlapAlpha = 0.0f;
    if (isStartOverlap)
    {
        overlapAlpha = saturate(1 - ((vId >> 1) * 0.2));
    }

    if (isEndOverlap && vId > 64)
    {
        overlapAlpha = saturate(1 - ((64 - (vId >> 1)) * 0.2));
    }

    if (vId & 0x1) {			// outside
        const float w1 = (indexData[iId + startOffset].B & 0x3fff) * 0.002f - 16.0f;
        output.posW = points[outsideLine].xyz + w1 * perpendicular;
    }
    else {					// inside
        const float w0 = ((indexData[iId + startOffset].B >> 14) & 0x3fff) * 0.002f - 16.0f;			// -32 .. 33.536m in mm resolution
        output.posW = points[insideLine].xyz + w0 * perpendicular;
    }

    output.texCoords = float3(1 - (vId & 0x1), points[outsideLine].w, t);
    if (isFlipped) output.texCoords.x *= -1;
    output.pos = mul(float4(output.posW, 1), viewproj);
    output.flags.y = materialFlag;
    output.colour.r = 1- abs(output.texCoords.x);
    output.colour.a = overlapAlpha;

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
	PS_OUTPUT output = (PS_OUTPUT)0;



    // get the materials
    uint material = vIn.flags.y;
    TF_material MAT =  materials[material];


	
	// couple of fixed materials
	if(material == 2045) 				// vertex colour blend - verge etc
	{
		output.Elevation.a = vIn.colour.a * smoothstep(0, 1, vIn.colour.r);
		output.Elevation.r = vIn.posW.y * output.Elevation.a ;
		return output;
	}
	
	if(material == 2046)  				// solid elevation only
	{
		output.Elevation.a = vIn.colour.a;
		output.Elevation.r = vIn.colour.a * vIn.posW.y;
		return output;
	}
	
	if(material == 2047)				// curvature
	{
		output.Elevation.a = 0;
		output.Elevation.r = 0.1 * cos((abs(vIn.texCoords.x)) * 1.57079632679);
		return output;
	}
	

    
	
	
	// uv's
    float2x2 rotate;
    sincos(MAT.uvRotation, rotate[0][1], rotate[0][0]);
    rotate[1][0] = -rotate[0][1];
    rotate[1][1] = rotate[0][0];
    float2 uv = mul(rotate, vIn.texCoords.xy * MAT.uvScale);

    sincos(MAT.uvWorldRotation, rotate[0][0], rotate[0][1]);
    rotate[1][0] = -rotate[0][1];
    rotate[1][1] = rotate[0][0];
    float2 uvWorld = mul(rotate, vIn.posW.xz / MAT.worldSize);
	
	
    // alpha
    float alpha = vIn.colour.a;
    if (MAT.useAlpha)
    {
        if (MAT.baseAlphaClampU)
        {
            float sideAlpha = smoothstep(0, MAT.uvScaleClampAlpha.x, abs(vIn.texCoords.x)) * (1 - smoothstep(MAT.uvScaleClampAlpha.y, 1, abs(vIn.texCoords.x)));


            //sideAlpha += range * (alphaDetail-0.5);

            if (MAT.baseAlphaScale > 0.05)
            {
                float2 uvAlpha = vIn.texCoords.xy * MAT.uvScale;    // without rotate
                uvAlpha.x = clamp(sideAlpha, 0.1, 0.9);
                float alphaBase = gmyTextures.T[MAT.baseAlphaTexture].Sample(SMP, uvAlpha).r;
                //if (uvAlpha.x > 0.89)  alphaBase = 1;

                sideAlpha = lerp(1, saturate((alphaBase + MAT.baseAlphaBrightness) * MAT.baseAlphaContrast), MAT.baseAlphaScale);
            }

            //float range = min(1 - sideAlpha, sideAlpha);
            float alphaDetail = gmyTextures.T[MAT.detailAlphaTexture].Sample(SMP, uvWorld).r;
            alpha *= lerp(1, saturate((sideAlpha + alphaDetail + MAT.detailAlphaBrightness) * MAT.detailAlphaContrast), MAT.detailAlphaScale);

            alpha *= sideAlpha;
        }
        else
        {
            float alphaBase = gmyTextures.T[MAT.baseAlphaTexture].Sample(SMP, uv).r;
            float alphaDetail = gmyTextures.T[MAT.detailAlphaTexture].Sample(SMP, uvWorld).r;

            alpha *= lerp(1, smoothstep(0, 1, vIn.colour.r), MAT.vertexAlphaScale);
            alpha *= lerp(1, saturate((alphaBase + MAT.baseAlphaBrightness) * MAT.baseAlphaContrast), MAT.baseAlphaScale);
            alpha *= lerp(1, saturate((alphaDetail + MAT.detailAlphaBrightness) * MAT.detailAlphaContrast), MAT.detailAlphaScale);
        }
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
		output.Elevation.r += (gmyTextures.T[MAT.baseElevationTexture].Sample(SMP, uv).r - MAT.baseElevationOffset) * MAT.baseElevationScale;
		output.Elevation.r += (gmyTextures.T[MAT.detailElevationTexture].Sample(SMP, uvWorld).r - MAT.detailElevationOffset) * MAT.detailElevationScale;
		
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
		float3 albedo = gmyTextures.T[MAT.baseAlbedoTexture].Sample(SMP, uv).rgb;
		float3 albedoDetail = gmyTextures.T[MAT.detailAlbedoTexture].Sample(SMP, uvWorld).rgb;
		
		float3 A = lerp(albedo, 0.5, saturate(MAT.albedoBlend));
		float3 B = lerp(albedoDetail, 0.5, saturate(-MAT.albedoBlend));
		
		output.Albedo.rgb = clamp(0.04, 0.9,  A * B * 4 * MAT.albedoScale );		// 0.04, 0.9 charcoal to fresh snow
		output.Albedo.a = alpha;
		
		
		float baseRoughness = gmyTextures.T[MAT.baseRoughnessTexture].Sample(SMP, uv).r;
		float detailRoughness = gmyTextures.T[MAT.detailRoughnessTexture].Sample(SMP, uvWorld).r;
		
		float rA = lerp(baseRoughness, 0.5, saturate(MAT.roughnessBlend));
		float rB = lerp(detailRoughness, 0.5, saturate(-MAT.roughnessBlend));
		
		output.PBR.rgb = saturate( pow(rA * rB * 2, MAT.roughnessScale) );
		output.PBR.a = alpha;
	}

    if (MAT.debugAlpha)
    {
        output.Albedo = float4(1, 0, 0, alpha);
    }

	
	output.Alpha=float4(1, 1, 1, 1);

	output.Ecotope1=float4(0, 0, 0, alpha);
	output.Ecotope2=float4(0, 0, 0, alpha);
	output.Ecotope3=float4(0, 0, 0, alpha);
	output.Ecotope4=float4(0, 0, 0, alpha);
	
	return output;
}
	

