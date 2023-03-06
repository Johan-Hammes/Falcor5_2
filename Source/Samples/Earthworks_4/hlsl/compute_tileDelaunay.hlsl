
#include "groundcover_defines.hlsli"
#include "terrainDefines.hlsli"
#include "terrainFunctions.hlsli"




Texture2D<float> gInHgt;
Texture2D<uint> gInVerts;

RWStructuredBuffer<Terrain_vertex> 	VB;
RWStructuredBuffer<gpuTile> 		tiles;

RWStructuredBuffer<t_DrawArguments> tileDrawargsArray;					//?? ek dink dit doen niks


cbuffer gConstants
{
	uint tile_Index;
};

float height(int2 coord)
{
	//return gInHgt[ coord +1 ].r;
	return ( gInHgt[ coord ].r + gInHgt[ coord + int2(-1, 0) ].r + gInHgt[ coord + int2(-1, -1) ].r + gInHgt[ coord + int2(0, -1) ].r ) * 0.25;
}


void addTriangles(uint A, uint B, uint C, uint D)
{

	uint offset = tile_Index * numVertPerTile;
	uint flag = ((uint)(A != B) << 3) + ((uint)(A != C) << 2) + ((uint)(D != B) << 1) + ((uint)(D != C) << 0);
	uint idx = 0;
			
	if (flag == 15) 		// we have 4
	{
		InterlockedAdd(tileDrawargsArray[tile_Index].instanceCount, 2, idx);	
		InterlockedAdd(tiles[tile_Index].numTriangles, 2, idx);	
		VB[offset + (idx * 3) + 0].idx = A;
		VB[offset + (idx * 3) + 1].idx = B;
		VB[offset + (idx * 3) + 2].idx = C;
		VB[offset + (idx * 3) + 0].hgt = height( unpack_tile_pos(A) * 2 );	// good argument to use rg8 format instaead
		VB[offset + (idx * 3) + 1].hgt = height( unpack_tile_pos(B) * 2);
		VB[offset + (idx * 3) + 2].hgt = height( unpack_tile_pos(C) * 2);


		idx ++;
		VB[offset + (idx * 3) + 0].idx = B;
		VB[offset + (idx * 3) + 1].idx = D;
		VB[offset + (idx * 3) + 2].idx = C;
		VB[offset + (idx * 3) + 0].hgt = height( unpack_tile_pos(B) * 2);
		VB[offset + (idx * 3) + 1].hgt = height( unpack_tile_pos(D) * 2);
		VB[offset + (idx * 3) + 2].hgt = height( unpack_tile_pos(C) * 2);
	}
	
	if (flag == 7 || flag == 11) 
	{
		InterlockedAdd(tileDrawargsArray[tile_Index].instanceCount, 1, idx);	
		InterlockedAdd(tiles[tile_Index].numTriangles, 1, idx);	
		VB[offset + (idx * 3) + 0].idx = B;
		VB[offset + (idx * 3) + 1].idx = D;
		VB[offset + (idx * 3) + 2].idx = C;
		VB[offset + (idx * 3) + 0].hgt = height( unpack_tile_pos(B) * 2);
		VB[offset + (idx * 3) + 1].hgt = height( unpack_tile_pos(D) * 2);
		VB[offset + (idx * 3) + 2].hgt = height( unpack_tile_pos(C) * 2);
	}

	if (flag == 13 || flag == 14) 
	{
		InterlockedAdd(tileDrawargsArray[tile_Index].instanceCount, 1, idx);	
		InterlockedAdd(tiles[tile_Index].numTriangles, 1, idx);	
		VB[offset + (idx * 3) + 0].idx = A;
		VB[offset + (idx * 3) + 1].idx = B;
		VB[offset + (idx * 3) + 2].idx = C;
		VB[offset + (idx * 3) + 0].hgt = height( unpack_tile_pos(A) * 2);
		VB[offset + (idx * 3) + 1].hgt = height( unpack_tile_pos(B) * 2);
		VB[offset + (idx * 3) + 2].hgt = height( unpack_tile_pos(C) * 2);
	}
	
}




[numthreads(tile_cs_ThreadSize, tile_cs_ThreadSize, 1)]
void main(uint2 coord : SV_DispatchThreadId)
{

	if (!any(coord))
	{
		tileDrawargsArray[tile_Index].instanceCount = 0;
	}
	AllMemoryBarrierWithGroupSync();
	
	
	if ((coord.x < 127) && (coord.y < 127))
	{
		uint A = gInVerts[coord];
		uint D = gInVerts[coord + uint2(1, 1)];
		if (A != D)
		{
			uint B = gInVerts[coord + uint2(0, 1)];
			uint C = gInVerts[coord + uint2(1, 0)];
			if (B != C)
			{
				addTriangles(A, B, C, D);
			}
		}
	}
	
	AllMemoryBarrierWithGroupSync();


}
		


/*
	uint offset = tile_Index * numVertPerTile;
	uint idx = 0;

	for (int y=0; y<127; y++){
		for (int x=0; x<127; x++){
		
			uint2 crd = uint2(x, y);
			uint A, B, C, D;
			A = gInVerts[crd];
			B = gInVerts[crd + uint2(0, 1)];
			C = gInVerts[crd + uint2(1, 0)];
			D = gInVerts[crd + uint2(1, 1)];
			
			if ((A != D) && (B != C))			// skip oorkruis punte
			{
				addTriangles(A, B, C, D, idx);
			}
		}
	}
	
	tiles[tile_Index].numTriangles = idx;

	
*/



