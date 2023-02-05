
Texture2D gInput;
Texture2D gNoise;
Texture2DArray gECTNoise;
Texture2DArray gColours;
RWTexture2D<float4> gOutput;

RWTexture2D<float> gOuthgt;
RWTexture2D<float4> gOutcolor;
RWTexture2D<uint> gOutPlants;


SamplerState linearSampler;		// for noise adnd colours

cbuffer gConstants
{
	float4 pixSize;			// 0	16		.x pixsize, .y numECT		
	float4 ect[32][5];		// 16	2560
	float2 plants[1024];	// 2576	8192
};

float myMOD(float x, float y)
{
	return x - y * floor(x / y);
}

float random(float2 p)
{
	const float2 r = float2( 23.1406926327792690, 2.6651441426902251);		// e^pi (Gelfond's constant)    ,    2^sqrt(2) (Gelfond–Schneider constant)
	return frac(3 *  cos(myMOD(123456789., 1e-7 + 256. * dot(p, r))));		// *200 seems tor pruduce good white noise
}


[numthreads(16, 16, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadId)
{
	uint2 crd = groupId.xy * 16 + groupThreadId.xy;
	
	float dx = gInput[crd + uint2(1, 0)].r - gInput[crd + uint2(-1, 0)].r;
	float dy = gInput[crd + uint2(0, 1)].r - gInput[crd + uint2(0, -1)].r;
	float3 a = float3(pixSize.r*2, dx, 0);
	float3 b = float3(0, dy, pixSize.r*2);
	float3 n = cross( a, b );
	n = normalize( n );
	float2 nA = normalize(n.xz);

	float hgt = gInput[crd].r;
	float rel = hgt - gInput[crd].g;
	float water = hgt - gInput[crd].b;
	float ectW[32];
	float ectWsum = 0;
	int numE = pixSize.g;
	int i;

	for (i = 0; i < numE; i++)		// ecotope section --------------------------------------------------------
	{
		float W1 = 1.0 + ect[i][0].r *(pow(0.5, pow(abs((hgt - ect[i][0].g) / ect[i][0].b), ect[i][0].a)) - 1);			// height
		float W2 = 1.0 + ect[i][1].r *(saturate((rel) / ect[i][1].g) - 1);												// relalat
		float W3 = 1.0 + ect[i][1].b *(1.0 - saturate(abs(rel) / ect[i][1].a) - 1);										// relalat lack of
		float W4 = 1.0 + ect[i][2].r *(1.0 - saturate((water) / ect[i][2].g) - 1);										// water table       .a is reserved fr numtrees
		float W5 = 1.0 + ect[i][3].r *(dot(nA, float2(ect[i][3].g, ect[i][3].b))  *saturate((1 + n.y) * 20) - 1);		// aspect
		float W6 = 1.0 + ect[i][4].r *(pow(0.5, pow(abs(((1 + n.y) - ect[i][4].g) / ect[i][4].b), ect[i][4].a)) - 1);	// slope
		float Wsum = W1 * W2 * W3 * W4 * W5 * W6;
		Wsum *= Wsum;
		ectW[i] = Wsum;
		ectWsum += Wsum;// ectW[i];
	}

	for (i = 0; i < numE; i++)		// normalize W --------------------------------------------------------
	{
		ectW[i] /= ectWsum;
		//uint3 crd3 = uint3(crd.x, crd.y, i);
		//gOutecotope[crd3] = ectW[i];	// save it so we can do trees outside for now
	}

	float3 texSize;
	gECTNoise.GetDimensions(texSize.x, texSize.y, texSize.z);
	float hgt_1 = 0;
	float hgt_2 = 0;
	float4 col_new = 0;
	float3 UV;
	float3 UV2;
	UV.xy = ((float2)crd + pixSize.zw) / texSize.xy * 512.0 / 496.0 * pixSize.r * 3.5 ;	// add offset position in here - will have to send in constant buffer

	//UV.xy *=10;
	UV2.xy = UV.xy / 32;
	for (i = 0; i < numE; i++)		// height NOISE --------------------------------------------------------
	{
		UV.z = i;
		UV2.z = i;
		hgt_1 += ectW[i] * (gECTNoise.SampleLevel(linearSampler, UV2, 0).r - 0.5) * 40;	// * scale + offset   bla bla, maybe float 16 data but still scale
		hgt_2 += ectW[i] * (gECTNoise.SampleLevel(linearSampler, UV, 0).r - 0.5) *3;	// * scale + offset   bla bla, maybe float 16 data but still scale
		col_new += ectW[i] * (gColours.SampleLevel(linearSampler, UV, 0));	// * scale + offset   bla bla, maybe float 16 data but still scale
	}

	gOuthgt[crd] = hgt + hgt_1 + hgt_2;
	gOutcolor[crd] = col_new;// .bgra;



	// Plants  - to FUR map and beyond  -----------------------------------------------------------------------------------------------
	float4 plantcolour = float4(0, 0, 1, 0.0);	// temp for display
	int currentPlant = 0;
	float noiseFloor = 0.01;		// dot start at zero, buf in rnd there
	float rnd = gNoise[crd].r;		// random((float2(crd.x, crd.y)) / 21512); //gNoise[crd].r;
	bool bP = false;
	gOutPlants[crd] = 0;
	for (i = 0; i < numE; i++)		
	{
		for (int pp = 0; pp < ect[i][2].a; pp++)
		{
			float chance = ectW[i] *  plants[currentPlant].x;
			if ( !bP && (rnd > noiseFloor) && rnd < (chance + noiseFloor) )			// fixme calculate a sum to repace this iff with that returns 0 if false
			{
				//gOutcolor[crd] = plantcolour;
				gOutPlants[crd] = currentPlant;		// FIXME ons misbruik hier 0 vir geen plante, dan moet ons altyd by 1 begin of so iets, dink daaroor
				bP = true;
			}
			noiseFloor += chance;
			currentPlant++;
		}
		plantcolour.g += 0.4;
	}



}
