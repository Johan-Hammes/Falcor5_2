
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



[numthreads(32, 1, 1)]
void main(uint dispatchId : SV_DispatchThreadId)
{
	uint t = dispatchId.x;
	

	

	if (t < 1001)		// FIXME hardcoded
	{	
	
	/*
		if ((tiles[t].flags >> 31) && tiles[t].numQuads > 0)
		{
			uint slot = 0;
			InterlockedAdd( feedback[0].numQuadTiles, 1, slot );
			
			uint totalQuads = tiles[t].numQuads;
			uint numB = (totalQuads >> 6) + 1;		// FIXME hardcoded move to header
			for (uint i = 0; i < numB; i++)
			{
				uint numPlants = min(totalQuads, 64);  // number of plants ion this block
				totalQuads -= 64;

				uint numLookupBlocks = 0;
				InterlockedAdd( feedback[0].numLookupBlocks_Quads, 1, numLookupBlocks );
				
				InterlockedAdd( DrawArgs_Quads[0].instanceCount, 64, slot );
				InterlockedAdd( feedback[0].numQuadBlocks, 1, slot );
				InterlockedAdd( feedback[0].numQuads, numPlants, slot );
				
		
				tileLookup[numLookupBlocks].tile = t + (numPlants <<16);
				tileLookup[numLookupBlocks].offset = t * numQuadsPerTile + (i * 64);
			}
		}
		*/
		
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
		//static uint unpackFrustum[1024] = (uint[1024])frustumflags;  
		
		tiles[t].flags = 0;
		//if ((unpackFrustum[t] & (1<<20)))
		//if ((frustumflags[t] & (1<<20)))
		{
			tiles[t].flags = 1<<31;
			
			uint slot = 0;
			InterlockedAdd( feedback[0].numTerrainTiles, 1, slot );
			
			
			
			DrawArgs_Terrain[0].instanceCount = 0;
			
			uint totalTriangles = tiles[t].numTriangles;
			uint numB = (totalTriangles >> 6) + 1;									// FIXME hardcoded
			for (uint i = 0; i < numB; i++)
			{
				uint numTriangles = min(totalTriangles, 64);  						// number of plants ion this block
				totalTriangles -= 64;

				uint numLookupBlocks = 0;
				InterlockedAdd( feedback[0].numLookupBlocks_Terrain, 1, numLookupBlocks );
				
				DrawArgs_Terrain[0].vertexCountPerInstance = 3;					// so a triangle - set this up once aerlier
				InterlockedAdd( DrawArgs_Terrain[0].instanceCount, 64, slot );
				InterlockedAdd( feedback[0].numTerrainBlocks, 1, slot );
				InterlockedAdd( feedback[0].numTerrainVerts, numTriangles, slot );
				
		
				terrainLookup[numLookupBlocks].tile = t + (numTriangles <<16);		// packing - mocve to function					;-) I think tiles[t].index  = t
				terrainLookup[numLookupBlocks].offset = t * numVertPerTile + (i * 64 * 3);	
			}
		}
	
	}
}


