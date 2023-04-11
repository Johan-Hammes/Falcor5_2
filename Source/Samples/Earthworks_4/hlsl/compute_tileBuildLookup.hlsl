
#include "groundcover_defines.hlsli"
#include "terrainDefines.hlsli"
				


RWStructuredBuffer<tileLookupStruct>	tileLookup;				// output
RWStructuredBuffer<tileLookupStruct>	plantLookup;			// output
RWStructuredBuffer<tileLookupStruct>	terrainLookup;			// output

RWStructuredBuffer<gpuTile> 			tiles;
StructuredBuffer<uint> 					visibility;

RWStructuredBuffer<GC_feedback>			feedback;

RWStructuredBuffer<t_DrawArguments> 	DrawArgs_Quads;
RWStructuredBuffer<t_DrawArguments> 	DrawArgs_Terrain;


cbuffer gConstants
{
	uint4 frustumflags[1024];
};


uint unpackFrustum(uint x)
{
    uint index = x >> 2;
    uint offset = x & 0x3;
    switch (offset)
    {
    case 0: return frustumflags[index].x;
    case 1: return frustumflags[index].y;
    case 2: return frustumflags[index].z;
    case 3: return frustumflags[index].w;
    }
    return 0;
}


[numthreads(32, 1, 1)]
void main(uint dispatchId : SV_DispatchThreadId)
{
	const uint t = dispatchId.x;
	

	if ((t>0) && (t < 1000))		// FIXME hardcoded
	{

        //if ((unpackFrustum(t) & (1 << 20)) && tiles[t].numQuads > 0)
        if ((unpackFrustum(t) >0) && tiles[t].numQuads > 0)
        //if (tiles[t].numQuads > 0)
		{
			
			uint totalQuads = tiles[t].numQuads;
            uint numB = (totalQuads >> 4) + 1;		// FIXME hardcoded move to header

            uint numQuadLookupBlocks;
            InterlockedAdd(feedback[0].numLookupBlocks_Quads, numB, numQuadLookupBlocks);

            uint slot = 0;
            InterlockedAdd(feedback[0].numQuadTiles, 1, slot);
            InterlockedAdd(DrawArgs_Quads[0].instanceCount, numB, slot);
            InterlockedAdd(feedback[0].numQuadBlocks, numB, slot);
            InterlockedAdd(feedback[0].numQuads, totalQuads, slot);

			for (uint i = 0; i < numB; i++)
			{
				uint numPlants = min(totalQuads, 16);  // number of plants on this block
				totalQuads -= 16;
				tileLookup[numQuadLookupBlocks + i].tile = t + (numPlants << 16);
				tileLookup[numQuadLookupBlocks + i].offset = (t * numQuadsPerTile) + (i * 16);
			}
		}
		
		
		/*
		// *** and now build the lookup table for the plants compute shader
		if (tiles[t].numPlants > 0)
		{
			uint slot = 0;
			InterlockedAdd( feedback[0].numPlantTiles, 1, slot );
			
			uint totalPlants = tiles[t].numPlants;
			uint numB = (totalPlants >> 6) + 1;		// FIXME hardcoded
			for (uint i = 0; i < numB; i++)
			{
				uint numPlants = min(totalPlants, 64);  // number of plants ion this block
				totalPlants -= 64;

				uint numLookupBlocks = 0;
				InterlockedAdd( feedback[0].numLookupBlocks_Plants, 1, numLookupBlocks );
				
				InterlockedAdd( feedback[0].numPlantBlocks, 1, slot );
				InterlockedAdd( feedback[0].numPlants, numPlants, slot );
				
		
				plantLookup[numLookupBlocks].tile = tiles[t].index + (numPlants <<16);		// packing - mocve to function					;-) I think tiles[t].index  = t
				plantLookup[numLookupBlocks].offset = tiles[t].index*2000 + (i * 64);		// FXIME the 2000 is hardcoded pass in
			}
		}
		*/
		
		
		/*
			So the really interesting part is that doing frustum culling does almost nothing for rendering speed, the vertex shader is just not the bottlenect
			and its a hell of a lot easier to just have one surface to bother with
		*/
		
		tiles[t].flags = 0;
		if ((unpackFrustum(t) & (1<<20)))
		{
			tiles[t].flags = 1<<31;
			
			uint totalTriangles = tiles[t].numTriangles;
			uint numB = (totalTriangles >> 6) + 1;									// FIXME hardcoded

            uint numLookupBlocks = 0;
            InterlockedAdd(feedback[0].numLookupBlocks_Terrain, numB, numLookupBlocks);

            uint slot = 0;
            InterlockedAdd(feedback[0].numTerrainTiles, 1, slot);
            InterlockedAdd(DrawArgs_Terrain[0].instanceCount, 64* numB, slot);
            InterlockedAdd(feedback[0].numTerrainBlocks, numB, slot);
            InterlockedAdd(feedback[0].numTerrainVerts, totalTriangles, slot);
            
			for (uint i = 0; i < numB; i++)
			{
				uint numTriangles = min(totalTriangles, 64);  						// number of plants ion this block
				totalTriangles -= 64;

				terrainLookup[numLookupBlocks + i].tile = t + (numTriangles <<16);		// packing - mocve to function					;-) I think tiles[t].index  = t
				terrainLookup[numLookupBlocks + i].offset = (t * numVertPerTile) + (i * 64 * 3);	
			}
		}
	}
}


