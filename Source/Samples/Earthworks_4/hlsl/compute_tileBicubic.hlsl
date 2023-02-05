


/*
	Bicubic interpolates the incoming elevation data to produce a smoother output to work from
	This is nessesary when the base data is 30m SRTM, but fast enough to leave it on for lidar
	
	try catmul clarke as well
	https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
*/



#include "terrainDefines.hlsli"


SamplerState linearSampler;
Texture2D gInput;
RWTexture2D<float> gOutput;
RWTexture2D<float4> gDebug;
RWTexture2D<float> gOuthgt_TEMPTILLTR;		// just here to replicate hieghts untill I add the rende to texture pass


cbuffer gConstants
{
	float2 offset;
	float2 size;
	float hgt_offset;
	float hgt_scale;
};


// running this in double presicion has amazing results, proving the problem is presision, but its 10X slower on my 980 card
float CubicHermite(float A, float B, float C, float D, float3 t) {
	float a = -A + (3.0 * B) - (3.0 * C) + D;
	float b = A * 2.0 - (5.0 * B) + 4.0 * C - D;
	float c = -A + C;
	float d = B * 2.0;

	return (a * t.z + b * t.y + c * t.x + d) * 0.5;
}

/*
[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadId)
{
    uint2 crd = (groupId.xy * tile_cs_ThreadSize) + groupThreadId.xy;
	
	float H[4][4];
	float2 texSize;
	gInput.GetDimensions(texSize.x, texSize.y);

	float2 F = frac((crd * size + frac(offset)) * texSize - 1.5);	// Using frac(offset) improves float presision
	int3 UV0 = int3(floor((crd * size + offset) * texSize - 1.5), 0);
	for (int y=0; y<4; y++) {
		for (int x = 0; x < 4; x++) {
			H[y][x] = gInput.Load(UV0 + int3(x, y, 0)) * hgt_scale;
		}
	}

	float3 t = F.x;
	t.y *= F.x;
	t.z = t.y * F.x;
	float CP0X = CubicHermite(H[0][0], H[0][1], H[0][2], H[0][3], t);
	float CP1X = CubicHermite(H[1][0], H[1][1], H[1][2], H[1][3], t);
	float CP2X = CubicHermite(H[2][0], H[2][1], H[2][2], H[2][3], t);
	float CP3X = CubicHermite(H[3][0], H[3][1], H[3][2], H[3][3], t);

	t = F.y;
	t.y *= F.y;
	t.z = t.y * F.y;
	float Hgt = CubicHermite(CP0X, CP1X, CP2X, CP3X, t)  + hgt_offset;
	
	gOutput[crd] = Hgt;
	gOuthgt_TEMPTILLTR[crd] = Hgt;
	
}
*/








[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadId)
{
    uint2 crd = (groupId.xy * tile_cs_ThreadSize) + groupThreadId.xy;
	
	float2 texSize;
	gInput.GetDimensions(texSize.x, texSize.y);
	float2 invTexSize = 1.0 / texSize;
	
	

	float2 iTc = ((crd - 4.0)  * size + offset) * texSize;
	float2 tc = floor(iTc);											// This term is very diffirent in pixel shader   floor(iTc - 0.5) + 0.5; for alf pixel offsets

	float2 f = iTc - tc;
	float2 f2 = f * f;
	float2 f3 = f2 * f; 

	float2 w0 = f2 - 0.5 * (f3 + f);
	float2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
	float2 w3 = 0.5 * (f3 - f2);
	float2 w2 = 1.0 - w0 - w1 - w3;

	float2 s0 = w0 + w1;
	float2 s1 = w2 + w3;

	float2 f0 = w1 / s0;
	float2 f1 = w3 / s1;

	float2 t0 = tc - 1 + f0;
	float2 t1 = tc + 1 + f1;

	t0 *= invTexSize;
	t1 *= invTexSize;

	float4 result =   (gInput.SampleLevel(linearSampler, float2(t0.x, t0.y), 0) * s0.x) * s0.y * hgt_scale
					+ (gInput.SampleLevel(linearSampler, float2(t1.x, t0.y), 0) * s1.x) * s0.y * hgt_scale
					+ (gInput.SampleLevel(linearSampler, float2(t0.x, t1.y), 0) * s0.x) * s1.y * hgt_scale
					+ (gInput.SampleLevel(linearSampler, float2(t1.x, t1.y), 0) * s1.x) * s1.y * hgt_scale;
					
	

	result.r += hgt_offset;
	//result.r =  gInput.SampleLevel(linearSampler, float2(t0.x, t0.y), 0);
	
	gOutput[crd] = result.r;
	gOuthgt_TEMPTILLTR[crd] = result.r;
	
	//float4 debug = result / 200.0f; 
	//debug.a = 1;
	//gDebug[crd] = debug;
	
	
}

