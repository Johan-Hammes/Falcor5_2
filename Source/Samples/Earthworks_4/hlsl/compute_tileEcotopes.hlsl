

#include "terrainDefines.hlsli"
#include "terrainFunctions.hlsli"

#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		


SamplerState linearSampler;		// for noise adnd colours

cbuffer gConstants
{
    int numEcotopes;
    int debug;
    float pixelSize;
    int lod;

    float2 lowResOffset;
    float lowResSize;
    uint tileIndex;

    int2 tileXY;
    float2 padd2;

    float4 ect[12][5];	  // 16		2560
    float4 texScales[12]; // texture size, displacement scale, pixSize, 0
    //uint4 totalDensity[12][16];
    //uint4 plantIndex[12][64];  // 2576	5648        // u16 better but not in constant buffer
};


Buffer<uint> plantIndex;
//Buffer<float> totalDensity;



Texture2D gHeight : register(u0);
RWTexture2D<float3> gAlbedo : register(u1);

Texture2D<float> gLowresHgt : register(t0);
Texture2D<float3> gInPermanence : register(t1);
Texture2D<float4> gInEct_0 : register(t2); //.. t5
Texture2D<float4> gInEct_1 : register(t3);
Texture2D<float4> gInEct_2 : register(t4);
Texture2D<float4> gInEct_3 : register(t5);

Texture2D<float4> gAlbedoAlpha[12] : register(t6);	  // these are dowbn to 12 so we can consider moving them closer togetehr again
Texture2D<float> gDisplacement[12] : register(t18);
Texture2D<float3> gPBR[12] : register(t30);
Texture2D<float3> gECTNoise[12] : register(t42);

Texture2D<uint> 	gNoise : register(t54);

// for plants
RWStructuredBuffer<gpuTile> 		tiles;
RWStructuredBuffer<instance_PLANT> 	quad_instance;
RWStructuredBuffer<GC_feedback>		feedback;





[numthreads(16, 16, 1)]
void main(uint2 crd : SV_DispatchThreadId)
{

    gpuTile tile = tiles[tileIndex];
    float OH = gHeight[uint2(128, 128)].r - (tile.scale_1024 * 2048);	// Its corner origin rather than middle
    tile.origin.y = OH;

    float3 permanence = gInPermanence[crd];
    if (any(permanence)) {

        // calculate the normal
        uint2 crd_clamped = clamp(crd, 1, tile_numPixels - 2);
        float dx = gHeight[crd_clamped + int2(-1, 0)].r - gHeight[crd_clamped + int2(1, 0)].r;
        float dy = gHeight[crd_clamped + int2(0, -1)].r - gHeight[crd_clamped + int2(0, 1)].r;
        float3 n = normalize(float3(dx, 2.0 * pixelSize, dy));
        float2 nA = n.xz;

        float hgt = gHeight[crd].r;
        //float2 lowResUV = (crd - 4.0) * lowResSize + lowResOffset;
        float2 lowResUV = (crd  ) * lowResSize + lowResOffset;
        float rel = hgt - (gLowresHgt.SampleLevel(linearSampler, lowResUV, 0) * padd2.y + padd2.x);

        float weights[12];
        float ectWsum = 0;
        const float4 inEct1 = gInEct_0[crd];
        const float4 inEct2 = gInEct_1[crd];
        const float4 inEct3 = gInEct_2[crd];
        const float4 inEct4 = gInEct_3[crd];
        weights[0] = inEct1.r + 0.001;
        weights[1] = inEct1.g + 0.001;
        weights[2] = inEct1.b + 0.001;

        weights[3] = inEct2.r + 0.001;
        weights[4] = inEct2.g + 0.001;
        weights[5] = inEct2.b + 0.001;

        weights[6] = inEct3.r + 0.001;
        weights[7] = inEct3.g + 0.001;
        weights[8] = inEct3.b + 0.001;

        weights[9] = inEct4.r + 0.001;
        weights[10] = inEct4.g + 0.001;
        weights[11] = inEct4.b + 0.001;

        if (debug < numEcotopes) {
            weights[debug] = 1;
        }



        int i;


        float2 World = (tileXY + ((crd - 4.0f) / 248.0f)) * pixelSize * 248.0f;


        float hgt_1 = 0;
        float hgt_2 = 0;

        if (permanence.b < 1.0 || (debug < numEcotopes) ) {

            for (i = 0; i < numEcotopes; i++) // ecotope weights calculation -----------------------------------------------------------------------------------
            {
                float2 UV = World / texScales[i].x;
                float2 UV2 = frac(World / 2048.0f);
                float MIP = log2(pixelSize / 0.005f);

                //height
                weights[i] *= lerp(1, smoothstep(ect[i][0].g - ect[i][0].a, ect[i][0].g + ect[i][0].a, hgt) * smoothstep(ect[i][0].b + ect[i][0].a, ect[i][0].b - ect[i][0].a, hgt), ect[i][0].r);

                //concavity
                weights[i] *= lerp(1, saturate((rel) / ect[i][1].g), ect[i][1].r);
                
                //flatness
                weights[i] *= lerp(1, 1.0 - saturate(abs(rel) / ect[i][1].a), ect[i][1].b);
                
                //slope
                weights[i] *= lerp(1, smoothstep(ect[i][2].g - ect[i][2].a, ect[i][2].g + ect[i][2].a, (1 - n.y)) * smoothstep(ect[i][2].b + ect[i][2].a, ect[i][2].b - ect[i][2].a, (1 - n.y)), ect[i][2].r);

                // aspect
                weights[i] *= lerp(1, saturate(dot(nA, float2(ect[i][3].g, ect[i][3].b))), ect[i][3].r);

                //weights[i] = saturate(rel / 50);

                /*
                if (ect[i][0].r) { // height
                    weights[i] *= 1.0 + ect[i][0].r * (pow(0.5, pow(abs((hgt - ect[i][0].g) / ect[i][0].b), ect[i][0].a)) - 1);
                }
                *

                // We have temprarily lost concavity and Flatness,as the low res texture is not being set. This was in the move to jpeg2000 - needs terrainmanager code to just load and set that
                // texture
                /*
                if (ect[i][1].r) { // concavity
                    weights[i] *= 1.0 + ect[i][1].r * (saturate((rel) / ect[i][1].g) - 1);
                }
                if (ect[i][1].b) { // flatness
                    weights[i] *= 1.0 + ect[i][1].b * (1.0 - saturate(abs(rel) / ect[i][1].a) - 1);
                }
                */

                /*
                if (ect[i][3].r) { // aspect
                    weights[i] *= 1.0 + ect[i][3].r * (dot(nA, float2(ect[i][3].g, ect[i][3].b)) - 1);
                }

                if (ect[i][4].r) { // slope
                    weights[i] *= 1.0 + ect[i][4].r * (pow(0.5, pow(abs(((1 - n.y) - ect[i][4].g) / ect[i][4].b), ect[i][4].a)) - 1);
                }

                if (ect[i][2].r) { // large area noise texture
                    float2 UV2 = UV;
                    UV2.xy *= ect[i][2].g;
                    float hgtTex = gAlbedoAlpha[i].SampleLevel(linearSampler, UV2, MIP - 6).a * 2.0; // FIXME mip-6 is ect[i][2].g dependent
                    weights[i] *= 1.0 + ect[i][2].r * (saturate((hgtTex * ect[i][2].b) + ect[i][2].a) - 1);
                    hgt_1 += ect[i][2].r * weights[i] * (hgtTex - 1.0) * 0.3;
                }
                if (ect[i][2].r) { // detail noise texture
                    float hgtTex = gAlbedoAlpha[i].SampleLevel(linearSampler, UV, MIP).a;
                    weights[i] *= hgtTex * 2;
                }
                
                // also scale by the weighs, and offset around 0.5

                weights[i] *= weights[i]; // sqr to exagerate the result, play with this some more, the idea seems good
                */
                ectWsum += weights[i];
            }
        }
        else {
            for (i = 0; i < numEcotopes; i++) // ecotope weights calculation -----------------------------------------------------------------------------------
            {
                ectWsum += weights[i];
            }
        }

        for (i = 0; i < numEcotopes; i++) // normalize the weights --------------------------------------------------------------------------------------
        {
            if (debug  == i) {
                gAlbedo[crd] = float3(lerp(0.0, 1.0, saturate((weights[i] * 2) - 1)), lerp(1.0, 0.0, abs(0.5 - weights[i]) * 2), lerp(0.3, 0.0, saturate(weights[i] * 2)));
            }
            weights[i] /= ectWsum;
        }


        float3 col_new = 0;
        for (i = 0; i < numEcotopes; i++)
        {
            float2 UV = World / texScales[i].x;
            float MIP = log2(pixelSize / 0.005f);  // There is a BUG in here I assume with the 0.005

            col_new += weights[i] * gAlbedoAlpha[i].SampleLevel(linearSampler, UV, MIP).rgb;
            float hgtTex = gDisplacement[i].SampleLevel(linearSampler, UV, MIP).r;
            hgt_2 += weights[i] * (hgtTex - 0.5) * texScales[i].g;
        }

        //float2 UV = World / texScales[0].x;
        //col_new = gAlbedoAlpha[0].SampleLevel(linearSampler, UV, 0).rgb;

        {
            //			float2 UV = World / texScales[4].x;
            //			float MIP = log2(pixelSize / 0.005f);
            //			col_new = weights[4] * gAlbedoAlpha[4].SampleLevel(linearSampler, UV, MIP).rgb;
        }

        //plants
        
        uint x_idx = (tileXY.x * 23 + crd.x);
        uint y_idx = (tileXY.y * 13 + crd.y);
        uint rnd = gNoise.Load(int3(x_idx & 0xff, y_idx & 0xff, 0));

        int sum = 0;
        int ecotopeForPlants = 20;
        
        for (i = 0; i < numEcotopes; i++)
        {
            uint density = plantIndex.Load(i * (16 * 65) + (lod * 65));
            sum += (int)((float)density * weights[0]);
            if (sum > rnd) {
                ecotopeForPlants = i;
                break;
            }
        }
        if (crd.x < 4)ecotopeForPlants = 20;;
        if (crd.x > 252)ecotopeForPlants = 20;;
        if (crd.y < 4)ecotopeForPlants = 20;;
        if (crd.y > 252)ecotopeForPlants = 20;;



        int offset = rnd & 0x3ff;
        if(ecotopeForPlants < numEcotopes)
        {
            const int thisplantIndex = plantIndex.Load(ecotopeForPlants * (16 * 65) + (lod * 65) + 1 + (offset >> 4));
            //const int thisplantIndex = plantIndex[ecotopeForPlants][offset >> 4].x;
            uint uHgt = (gHeight[crd].r - OH) / tile.scale_1024;

            uint slot = 0;
            InterlockedAdd(tiles[tileIndex].numQuads, 1, slot);
            feedback[0].plants_culled = slot;

            quad_instance[tileIndex * numQuadsPerTile + slot].xyz = pack_pos(crd.x-4, crd.y-4, uHgt, rnd);				// FIXME - redelik seker die is verkeerd -ek dink ek pak 2 extra sub pixel bits 
            quad_instance[tileIndex * numQuadsPerTile + slot].s_r_idx = pack_SRTI(1, rnd, tileIndex, thisplantIndex);			    //(1 << 31) + (child_idx << 11) + (0);
        }
        
        // write final colours
        if (debug > 100) {
            //gHeight[crd] = hgt + (hgt_1 + hgt_2);//			*permanence.r;
            gAlbedo[crd] = lerp(gAlbedo[crd], col_new, permanence.g);
            //gAlbedo[crd] = gInEct[1][crd].rgb;
        }
    }
}
