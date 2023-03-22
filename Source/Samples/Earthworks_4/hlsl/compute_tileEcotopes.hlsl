

#include "terrainDefines.hlsli"
#include "terrainFunctions.hlsli"

SamplerState linearSampler;		// for noise adnd colours

cbuffer gConstants
{
    int numEcotopes;
    int debug;
    float pixelSize;
    float padd;

    float2 lowResOffset;
    float lowResSize;
    float padd_b;

    int2 tileXY;
    float2 padd2;

    float4 ect[12][5];	  // 16		2560
    float4 texScales[12]; // texture size, displacement scale, pixSize, 0
    float2 plants[1024];  // 2576	8192
};


Texture2D gHeight : register(u0);
RWTexture2D<float3> gAlbedo : register(u1);

Texture2D<float> gLowresHgt : register(t0);
Texture2D<float3> gInPermanence : register(t1);
Texture2D<float4> gInEct[4] : register(t2); //.. t5

Texture2D<float4> gAlbedoAlpha[12] : register(t6);	  // these are dowbn to 12 so we can consider moving them closer togetehr again
Texture2D<float> gDisplacement[12] : register(t18);
Texture2D<float3> gPBR[12] : register(t30);
Texture2D<float3> gECTNoise[12] : register(t42);





[numthreads(16, 16, 1)]
void main(uint2 crd : SV_DispatchThreadId)
{

    float3 permanence = gInPermanence[crd];
    if (any(permanence)) {

        // calculate the normal
        uint2 crd_clamped = clamp(crd, 1, tile_numPixels - 2);
        float dx = gHeight[crd_clamped + int2(-1, 0)].r - gHeight[crd_clamped + int2(1, 0)].r;
        float dy = gHeight[crd_clamped + int2(0, -1)].r - gHeight[crd_clamped + int2(0, 1)].r;
        float3 n = normalize(float3(dx, 2.0 * pixelSize, dy));
        float2 nA = n.xz;

        float hgt = gHeight[crd].r;
        float2 lowResUV = (crd - 4.0) * lowResSize + lowResOffset;
        float rel = hgt - gLowresHgt.SampleLevel(linearSampler, lowResUV, 0);

        float weights[12];
        float ectWsum = 0;
        const float4 inEct1 = gInEct[0][crd];
        const float4 inEct2 = gInEct[1][crd];
        const float4 inEct3 = gInEct[2][crd];
        const float4 inEct4 = gInEct[3][crd];
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



        int i;


        float2 World = (tileXY + ((crd - 4.0f) / 248.0f)) * pixelSize * 248.0f;


        float hgt_1 = 0;
        float hgt_2 = 0;

        if (permanence.b < 1.0) {

            for (i = 0; i < numEcotopes; i++) // ecotope weights calculation -----------------------------------------------------------------------------------
            {
                float2 UV = World / texScales[i].x;
                float2 UV2 = frac(World / 2048.0f);
                float MIP = log2(pixelSize / 0.005f);

                if (ect[i][0].r) { // height
                    weights[i] *= 1.0 + ect[i][0].r * (pow(0.5, pow(abs((hgt - ect[i][0].g) / ect[i][0].b), ect[i][0].a)) - 1);
                }

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
            if ((debug - 1) == i) {
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

        // write final colours
        if (!debug) {
            //gHeight[crd] = hgt + (hgt_1 + hgt_2);//			*permanence.r;
            gAlbedo[crd] = lerp(gAlbedo[crd], col_new, permanence.g);
            //gAlbedo[crd] = gInEct[1][crd].rgb;
        }
    }
}
