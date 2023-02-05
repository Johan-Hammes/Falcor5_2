

#include "terrainDefines.hlsli"
#include "terrainFunctions.hlsli"
#include "groundcover_defines.hlsli"

SamplerState linearSampler;		// for noise and colours

Texture2D<float> 	gInHgt;
RWTexture2D<float> 	gMip1;
RWTexture2D<float> 	gMip2;
RWTexture2D<float> 	gMip3;

RWTexture2D<float4> gDebug;
	
	
groupshared float H2[64][64];

	
[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]
void main(uint2 coord : SV_DispatchThreadId)
{
	
	
	
	float2 uv = coord / 64.0;			//* 0.015625;
	float h1 = gInHgt.SampleLevel(linearSampler, uv + float2(0.00390625, 0.00390625), 0);
	float h2 = gInHgt.SampleLevel(linearSampler, uv + float2(0.01171875, 0.00390625), 0);
	float h3 = gInHgt.SampleLevel(linearSampler, uv + float2(0.00390625, 0.01171875), 0);
	float h4 = gInHgt.SampleLevel(linearSampler, uv + float2(0.01171875, 0.01171875), 0);
	
	gMip1[coord*2 + uint2(0, 0)] = h1;
	gMip1[coord*2 + uint2(1, 0)] = h2;
	gMip1[coord*2 + uint2(0, 1)] = h3;
	gMip1[coord*2 + uint2(1, 1)] = h4;
	
	gDebug[coord*2 + uint2(0, 0)] = float4(frac(h1/10), frac(h1/100), frac(h1/1000), 1);
	gDebug[coord*2 + uint2(1, 0)] = float4(frac(h2/10), frac(h2/100), frac(h2/1000), 1);
	gDebug[coord*2 + uint2(0, 1)] = float4(frac(h3/10), frac(h3/100), frac(h3/1000), 1);
	gDebug[coord*2 + uint2(1, 1)] = float4(frac(h4/10), frac(h4/100), frac(h4/1000), 1);
	
	h1 += (h2 + h3 + h4);
	h1 *= 0.25;
	gMip2[coord] = h1;
	H2[coord.x][coord.y] = h1; 
	gDebug[coord + uint2(128, 0)] = float4(frac(h1/10), frac(h1/100), frac(h1/1000), 1);

	GroupMemoryBarrierWithGroupSync();
	if (((coord.x & 0x1) == 0)  && ((coord.y & 0x1) == 0))
	{
		uint2 C3 = coord / 2;
		gMip3[C3] = (H2[coord.x][coord.y] + H2[coord.x+1][coord.y] + H2[coord.x][coord.y+1] + H2[coord.x+1][coord.y+1]) * 0.25;
		h1 = (H2[coord.x][coord.y] + H2[coord.x+1][coord.y] + H2[coord.x][coord.y+1] + H2[coord.x+1][coord.y+1]) * 0.25;
		gDebug[C3] = float4(frac(h1/10), frac(h1/100), frac(h1/1000), 1);
	}
	
}

