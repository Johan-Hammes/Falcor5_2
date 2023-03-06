

#include "terrainDefines.hlsli"
#include "terrainFunctions.hlsli"


Texture2D<uint>		gInVerts;
RWTexture2D<uint>	gOutVerts;
RWTexture2D<float4> gDebug;


cbuffer gConstants
{
	uint step;
};


[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]
void main(int2 crd : SV_DispatchThreadId)
{
	float dstSqr;
	float smallest = 500000;
	uint V;
	
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			V = gInVerts[crd + int2(x, y)*step];
			if (V != 0) 
			{
                const int2 delta = int2(V & 0x7f, (V >> 7) & 0x7f) - crd;
                dstSqr = dot(delta, delta);
				if (dstSqr  < smallest) {
					smallest = dstSqr;
					gOutVerts[crd] = V;
					//gDebug[crd] = float4(frac((V & 0x7f) * 10.30), frac((V >> 7) * 10.0345), frac((V & 0x7f) * 257 / (V >> 7)), 1);		// FIXME TEMPORARY	- good for debug but it doubles trhe time that the algorithm datkre - Its all data throuhghjput
				}
			}
		}
	}
}





/*

[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadId)
{
    uint2 crd = (groupId.xy * tile_cs_ThreadSize) + groupThreadId.xy;
    float dstSqr;
    float smallest = 500000;
    uint V;



    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            V = gInVerts[crd + int2(x, y)*step];
            if (V != 0)
            {
                //if (step == 2)gDebug[crd] = float4(1, 0, 0, 1);
                uint2 v_crd = unpack_tile_pos( V )/2;
                //uint zX = (V >> 9) & 0x1ff;
                //uint zY = V & 0x1ff;
                int dX = (int)v_crd.x - (int)crd.x -1;
                int dY = (int)v_crd.y - (int)crd.y -1;
                dstSqr = dX*dX + dY*dY;
                if (dstSqr  < smallest) {
                    smallest = dstSqr;
                    gOutVerts[crd] = V;
                    //gDebug[crd] = float4(frac(v_crd.x*10.30), frac(v_crd.y*10.0345), frac(v_crd.x * 257 / v_crd.y), 1);		// FIXME TEMPORARY	- good for debug but it doubles trhe time that the algorithm datkre - Its all data throuhghjput
                }
            }
        }
    }
    //gDebug[crd] = float4(1, 0, 0, 1);
}


*/
