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
	float4 data[2][4];	//[center, outside][p0, p1, p2, p3]  //128 bytes
};
// [2][4] fully contained  float 128, u16 64

// If we save this as a left and a right matrix. and save a float4(t)  Its a single matrix multiply for the answer
// 200,000 beziers -> (128) 25Mb, lot but not too bad, and probably perfect if split
// expanded 32X -> 2xfloat4 (32) X 32 = (1024)
// SO this is SUPER interesting - Turns out beziers MAY NOT be the best way to do this at runtime It is only a 1:8 improvement on memory [32 splits],
// we could optimally sample on curvature, and use significantly less than 32 splits on average
// However 4 splits would be same data rate, so maybe still good

struct bezierLayer
{
	uint A;			// flags, material, index [4][14][16]			combine with rootindex in constant buffer
	uint B;			// w0, w1 [4][14][14]
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


#define outsideLine 	(indexData[ iId ].A >> 31) & 0x1
#define insideLine 		(indexData[ iId ].A >> 30) & 0x1

#define isStartOverlap 	(indexData[ iId ].B >> 31) & 0x1
#define isEndOverlap 	(indexData[ iId ].B >> 30) & 0x1
#define isQuad 			(indexData[ iId ].B >> 29) & 0x1
#define isOverlap		(isStartOverlap) || (isEndOverlap)

// 12 bits left 4096 materials
#define materialFlag  	(indexData[ iId ].A >> 17) & 0x7ff		
#define index  			indexData[ iId ].A & 0x1ffff



splineVSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    splineVSOut output;
	
	float4 points[2];
    float t = (vId >> 1) / 64.0;
    const cubicDouble s0 = splineData[index];
	points[0] = cubic_Casteljau(t, s0.data[0][0], s0.data[0][1], s0.data[0][2], s0.data[0][3]);
	points[1] = cubic_Casteljau(t, s0.data[1][0], s0.data[1][1], s0.data[1][2], s0.data[1][3]);

	float3 perpendicular = normalize(points[1].xyz - points[0].xyz);

	if (vId & 0x1 ) {			// outside
        const float w1 = (indexData[iId].B & 0x3fff) * 0.002f - 16.0f;
        output.posW = points[outsideLine].xyz + w1 * perpendicular;
	} else {					// inside
        const float w0 = ((indexData[iId].B >> 14) & 0x3fff) * 0.002f - 16.0f;			// -32 .. 33.536m in mm resolution
        output.posW = points[insideLine].xyz + w0 * perpendicular;
	}
	
	output.texCoords = float3(vId & 0x1, points[outsideLine].w, t );
	output.pos =  mul( float4(output.posW, 1), viewproj);
	output.flags.y = materialFlag;
    output.colour = 1;
	
	return output;
}









float shevrons(float2 UV)
{
    if (UV.x < 0.02) return 0.9;
    if (UV.x > 0.98) return 0.9;

    float newU = abs(0.5 - UV.x) * 3;
    if (newU < 0.5) {
        float newV = frac(1 * (UV.y + newU * 0.3));
        if (newV > 0.93) {
            return 0.7;
        }
    }


    if (frac(UV.y * 32) > 0.5)
    {
        if (frac(UV.x * 2) > 0.5)return 0.1;
    }
    else
    {
        if (frac(UV.x * 2) <= 0.5)return 0.1;
    }

    return 0;
}


float4 dots(float2 UV)
{
    if (frac(UV.y * 5) < 0.25) return float4(1, 1, 1, 1);
    if (UV.x > 0.4 && UV.x < 0.6)return float4(1, 1, 1, 1);
    return float4(0, 0, 0, 0.5);
}





float4 psMain(splineVSOut vIn) : SV_TARGET0
{
	float4 colour = vIn.colour;
	uint material = vIn.flags.y;



    if (material > 2040) {
        // Curvature
        return 0;
    }
	
	// EDITING MATERIALS
	
	// middle line dotted white
	if(material == 800) {
        return dots(vIn.texCoords.xy);
	}
	
	// middle line dotted red
	if(material == 801) {
        return dots(vIn.texCoords.xy) * float4(1, 0, 0, 1);
	}
	
	// green shevron
	if(material == 802) {
        float A = shevrons(vIn.texCoords.xz);
        if (vIn.texCoords.z < 0.02 && vIn.texCoords.x > 0.5) return 0.8;
        if (vIn.texCoords.z > 0.98 && vIn.texCoords.x > 0.5) return 0.8;
        if (A < 0.01) return float4(saturate((vIn.flags.z) / 150.0f - 0.5) * 10, saturate((vIn.flags.w) / 50.0f - 0.0), saturate((vIn.flags.w) / 50.0f - 0.0), 0.19);
        return float4(0.0, 0.4, 0.0, A);
	}
	
	// blue shevron
	if(material == 803) {
        float A = shevrons(vIn.texCoords.xz);
        if (vIn.texCoords.z < 0.02 && vIn.texCoords.x > 0.5) return 0.8;
        if (vIn.texCoords.z > 0.98 && vIn.texCoords.x > 0.5) return 0.8;
        if (A < 0.01) return float4(saturate((vIn.flags.z) / 150.0f - 0.5) * 10, saturate((vIn.flags.w) / 50.0f - 0.0), saturate((vIn.flags.w) / 50.0f - 0.0), 0.19);
        return float4(0.0, 0.0, 0.5, A);
	}
	
	
	
	
	
	
	
	
	// selection
	if(material == 999) {
        return dots(vIn.texCoords.xy * 3) * float4(1, 1, 0, 1);
	}
	
	if(material == 1000) {
		// Curvature
		return float4(0.0, 0.0, 0.0, 0.0);
	}
	
	if(material == 1001) {
		// Curvature
		return float4(1.0, 0.0, 0.0, 1.0);
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
			alpha_uv.x = clamp(alpha_uv.x, 0.05, 0.95);
		}
        float alphaBase = gmyTextures.T[MAT.baseAlphaTexture].Sample(SMP, alpha_uv).r;
        float alphaDetail = gmyTextures.T[MAT.detailAlphaTexture].Sample(SMP, uvWorld).r;
		
		if (MAT.baseAlphaClampU && alpha_uv.x > 0.9 )
		{
			alphaBase = 1;
		}
		
		alpha = lerp(1,  smoothstep(0, 1, vIn.colour.r), MAT.vertexAlphaScale);
		alpha *=  lerp(1, saturate((alphaBase + MAT.baseAlphaBrightness) * MAT.baseAlphaContrast ), MAT.baseAlphaScale);
		alpha *=  lerp(1, saturate((alphaDetail + MAT.detailAlphaBrightness) * MAT.detailAlphaContrast  ), MAT.detailAlphaScale);
	}
	
	
	
	if(MAT.useColour)
	{
        float3 albedo = gmyTextures.T[MAT.baseAlbedoTexture].Sample(SMP, uv).rgb;
        float3 albedoDetail = gmyTextures.T[MAT.detailAlbedoTexture].Sample(SMP, uvWorld).rgb;
		
		float3 A = lerp(albedo, 0.5, saturate(MAT.albedoBlend));
		float3 B = lerp(albedoDetail, 0.5, saturate(-MAT.albedoBlend));
		
		colour.rgb = clamp(0.04, 0.9,  A * B * 4 * MAT.albedoScale );		// 0.04, 0.9 charcoal to fresh snow
		colour.a = alpha;
	}
	else
	{ 
		alpha = 0.0;
	}
	
    
    //colour.rgb = bindlesstextures[0].Sample(SMP, uv).rgb;

    //colour.rgb = gmyTextures.T[5].Sample(SMP, uv).rgb;

	//colour.rgb *= 0.8f;
    colour.a = alpha;

	return colour;





	
	switch(material){
		case 0:
			colour = float4(0, 0, 0, 0.3);
			break;
		case 1:
			float newV = frac( 2 * (vIn.texCoords.y));
			if (newV > 0.5) {
				colour = float4(0.3, 0.3, 0.3, 0.3);
			}
			break;
		case 2:
			colour = float4(0.5, 0.5, 0, 0.2);
			break;
		case 3:
			// driving lanes
			colour = float4(0, 0, 0.5, shevrons(vIn.texCoords.xy));
			break;
		case 4:
			colour = float4(0, 0.5, 0.0, shevrons(vIn.texCoords.xy));
			break;
		case 5:
			colour = float4(0, 0, 0, 0.1);
			break;
			
		case 11:
			//float newV = frac( 8 * (vIn.texCoords.y));
			colour = float4(0.9, 0.9, 0.0, 0.05 + 0.3 * smoothstep(0, 1, 1-vIn.texCoords.x));
			//if (frac( 8 * (vIn.texCoords.y)) > 0.5) colour.r = 0.2;
			break;
		case 10:
			float a = 1.0;
			colour = float4(0.05, 0.03, 0.02, a);
			if (frac( 8 * (vIn.texCoords.y)) > 0.95) colour = float4(0.01, 0.01, 0.01, a);
			
			
			//if (newV > 0.5)
			break;
			
			
		// robot colour coded 
		// RED
		case 20:
			float a1 = 0.04 * smoothstep(0, 1, 1-vIn.texCoords.x) + saturate((vIn.texCoords.x - 0.95) * 10);
			colour = float4(0.9, 0.0, 0, a1);
			break;
			
		// YELLOW	
		case 21:
			float a2 = 0.04 * smoothstep(0, 1, 1-vIn.texCoords.x) + saturate((vIn.texCoords.x - 0.95) * 10);
			colour = float4(0.9, 0.9, 0, a2);
			break;
			
		// GREEN	
		case 22:
			float alphaG = 0.4 * smoothstep(0, 1, 1-vIn.texCoords.x) + saturate((vIn.texCoords.x - 0.95) * 10);
			colour = float4(0.0, 0.9, 0, alphaG);
			break;
			
			
		// AMBER SELECTION
		case 23:
			colour = float4(0.07, 0.01, 0.0, 0.8);
			break;
			
			
		
		// lines on the road		 ----------------------------------------------------------------
		// solid white
		case 50:
			//colour = float4(0.0, 0.0, 0, 1);
			break;
			
		// dotted white
		case 51:
			float newVb = frac( 1 * (vIn.texCoords.y));
			float aa = newVb > 0.5;
			colour = float4(0.2, 0.2, 0.2, aa);
			break;
			
		case 52:
			colour = float4(0.2, 0.2, 0.2, 1);
			break;
			
		case 53:
			colour = float4(0.2, 0.2, 0.0, 1);
			break;
			
		case 54:
			float innerU = frac((vIn.texCoords.y - vIn.texCoords.x * 0.8)  * 3);
			float alphaInner = 1.0f;
			if (innerU < 0.5){
				alphaInner = 0.0f;
			}
			if (vIn.texCoords.x < 0.07) alphaInner = 1.0f;
			if (vIn.texCoords.x > 0.93) alphaInner = 1.0f;
			colour = float4(0.2, 0.2, 0.2, alphaInner);
			break;
			
		/*	
		// dotted + solid LEFT white
		case 53:
			float newV = frac( 2 * (vIn.texCoords.y));
			if (newV > 0.5) {
				colour = float4(0.3, 0.3, 0.3, 0.3);
			}
			break;
			
		// dotted+solid RIGTH white
		case 54:
			float newV = frac( 2 * (vIn.texCoords.y));
			if (newV > 0.5) {
				colour = float4(0.3, 0.3, 0.3, 0.3);
			}
			break;
		*/
	}
	
	
	
	//if (vIn.texCoords.x > 0.85)
	{
		if (vIn.texCoords.z < 0.02) colour.rgb = float3(0.15, 0.15, 0.05);
		if (vIn.texCoords.z > 0.98) colour.rgb = float3(0.15, 0.15, 0.05);
	}
	

	
	
	return colour;
}


	
