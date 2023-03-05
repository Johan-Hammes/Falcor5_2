

#include "terrainDefines.hlsli"
#include "terrainFunctions.hlsli"
#include "groundcover_defines.hlsli"



SamplerState linearSampler;		// for noise and colours

Texture2D<float> 					gInHgt;
RWTexture2D<uint> 					gOutVerts;
RWTexture2D<float4> 				gDebug;
RWStructuredBuffer<centerFeedback> 	tileCenters;
RWStructuredBuffer<gpuTile> 		tiles;



cbuffer gConstants
{
	float4 constants;			// .x pixsize
};


	
	
[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]	
void main(uint2 coord : SV_DispatchThreadId)
{

	if (coord.x == 127 && coord.y == 127) {
		uint tileIdx = constants.w;
		float centerHeight = gInHgt.SampleLevel(linearSampler, float2(0.5, 0.5), 0);
		tileCenters[tileIdx].min = centerHeight;
		tiles[tileIdx].origin.y = centerHeight - (tiles[tileIdx].scale_1024 * 2048);	// Its corner origin rather than middle
	}

   

	float pixSize = constants.x;
	float scale = constants.y;
	int offset = constants.z;

    int STEP = 4;

	for (int lod=0; lod<3; lod++)
	{
		int mask = 0x3 >> lod;
		int2 Vcrd = coord + mask;

        if (gOutVerts[Vcrd] > 0)
        {
            uint2 dCrd = Vcrd * 2 + 1;
            gDebug[dCrd] = float4(1, 1, 1, 1);
        }

		if (!any(coord & mask) && Vcrd.x < 127 && Vcrd.y < 127) {
			float2 uv = (Vcrd + 1.0) / 128.0f;
			float mip = 2-lod;

            int3 Hcrd = int3(Vcrd * 2 + 1, 0);
            float hAVS = 0;
            //  H[y][x] = gInput.Load(UV0 + int3(x, y, 0)) * hgt_scale;
            hAVS += gInHgt.Load(int3(Hcrd.x - 1, Hcrd.y - 1, 0));
            hAVS += gInHgt.Load(int3(Hcrd.x + 1, Hcrd.y - 1, 0));
            hAVS += gInHgt.Load(int3(Hcrd.x - 1, Hcrd.y + 1, 0));
            hAVS += gInHgt.Load(int3(Hcrd.x + 1, Hcrd.y + 1, 0));

            float hgt = gInHgt.Load(int3(Hcrd.x, Hcrd.y, 0));

			//if (abs( gInHgt.SampleLevel(linearSampler, uv, mip) - gInHgt.SampleLevel(linearSampler, uv, mip+1) ) > pixSize/5)
            if (abs(hgt - (hAVS / 4.0)) > pixSize / 15)
			{
                uint idx;
                InterlockedAdd(tiles[constants.w].numVerticis, 1, idx);
                if (idx < 1000)
                {

                    // test just pull to the edge
                    int edge = mask + 2; //???
                    if (Vcrd.x < edge)				Vcrd.x = 0;
                    if ((126 - Vcrd.x) < edge) 		Vcrd.x = 126;
                    if ((Vcrd.y) < edge) 			Vcrd.y = 0;
                    if ((126 - Vcrd.y) < edge) 		Vcrd.y = 126;

                    gOutVerts[Vcrd] = pack_tile_pos(Vcrd * 2 + 2);

#if defined(COMPUTE_DEBUG_OUTPUT)
                    uint2 dCrd = Vcrd * 2 + 1;
                    if (lod == 0) {
                        //gDebug[dCrd] = float4(0, 0.6, 1, 1);
                    }
                    if (lod == 1) {
                        //gDebug[dCrd] = float4(0, 1, 0.0, 1);
                    }
                    if (lod == 2) {
                        //gDebug[dCrd] = float4(1, 0.2, 0.0, 1);
                    }
#endif
                }
			}
		}
	}
}

