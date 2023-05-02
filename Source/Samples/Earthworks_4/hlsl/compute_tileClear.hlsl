#include "groundcover_defines.hlsli"


RWStructuredBuffer<GC_feedback>			feedback;

RWStructuredBuffer<t_DrawArguments> 	DrawArgs_Quads;
RWStructuredBuffer<t_DrawArguments> 	DrawArgs_Terrain;
RWStructuredBuffer<t_DrawArguments> 	DrawArgs_Plants;
RWStructuredBuffer<t_DrawArguments> 	DrawArgs_ClippedLoddedPlants;
RWStructuredBuffer<t_DispatchArguments> DispatchArgs_Plants;




[numthreads(1, 1, 1)]
void main(uint dispatchId : SV_DispatchThreadId)
{
	// Clear the feedback
	// -----------------------------------------------------------------------------------------------------
	feedback[0].numQuadTiles = 0;
	feedback[0].numQuadBlocks = 0;
	feedback[0].numQuads = 0;
    feedback[0].maxQuads = 0;

	feedback[0].numPlantTiles = 0;
	feedback[0].numPlantBlocks = 0;
	feedback[0].numPlants = 0;
    feedback[0].maxPlants = 0;
    feedback[0].numPostClippedPlants = 0;
	
	feedback[0].numTerrainTiles = 0;
	feedback[0].numTerrainBlocks = 0;
	feedback[0].numTerrainVerts = 0;
    feedback[0].maxTriangles = 0;
	
	feedback[0].numLookupBlocks_Quads = 0;
	feedback[0].numLookupBlocks_Plants = 0;
	feedback[0].numLookupBlocks_Terrain = 0;


    // Clear the draw arguments
    // -----------------------------------------------------------------------------------------------------
    DrawArgs_Terrain[0].instanceCount = 0;
    DrawArgs_Terrain[0].vertexCountPerInstance = 3;

    DrawArgs_Quads[0].instanceCount = 0;
    DrawArgs_Quads[0].vertexCountPerInstance = 16;

    DrawArgs_Plants[0].instanceCount = 0;
    DrawArgs_Plants[0].vertexCountPerInstance = 128;

    DrawArgs_ClippedLoddedPlants[0].instanceCount = 0;
    DrawArgs_ClippedLoddedPlants[0].vertexCountPerInstance = 128;

    DispatchArgs_Plants[0].numGroupX = 0;
    DispatchArgs_Plants[0].numGroupY = 1;
    DispatchArgs_Plants[0].numGroupZ = 1;
    DispatchArgs_Plants[0].padd = 0;

    
}
