
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"	
#include "terrainDefines.hlsli"
			


Texture2D<float> 	gHgt;
Texture2D<uint> 	gNoise;





RWStructuredBuffer<instance_PLANT> 		quad_instance;			// input / output
RWStructuredBuffer<instance_PLANT> 		plant_instance;			// output



RWStructuredBuffer<tileLookupStruct>	tileLookup;				// output
RWStructuredBuffer<gpuTile> 			tiles;



StructuredBuffer<tileExtract>			tileInfo;				// FIXME DOES NTO EXIST ANY MOR EMAKE SURE WE GET THE INDEX
RWStructuredBuffer<GC_feedback>			feedback;



cbuffer gConstants			// ??? wonder if this just works here, then we can skip structured buffer for it
{
	uint child_index;
	uint dX;
	uint dY;
};




[numthreads(256, 1, 1)]			
void main(uint dispatchId : SV_DispatchThreadId)
{
	gpuTile tile = tiles[ tileInfo[0].index ];
	
	
	if(dispatchId < tile.numQuads )
	{
		gpuTile tileC = tiles[child_index];
		
		uint XYZ = quad_instance[tileInfo[0].index * numQuadsPerTile + dispatchId].xyz;
		uint SRTI = quad_instance[tileInfo[0].index * numQuadsPerTile + dispatchId].s_r_idx;
		uint cx = XYZ >> 31;
		uint cy = (XYZ >> 21) & 0x1;
		
		if ((cx == dX) && (cy == dY))	//write cleaner		XOR
		//if ((cx == dX) && (cy == dY) && dX==1 && dY == 0)	//write cleaner		XOR
		{
		
			uint x = ((XYZ >> 22) & 0x1ff) + 16*dX - 8;	// remaining 9 bits
			uint y = ((XYZ >> 12) & 0x1ff) + 16*dY - 8;
			

			
			// noise only
			uint x_idx = (tile.lod * tile.X * 23 + x);
			uint y_idx = (tile.lod * tile.Y * 13 + y);
			uint rnd = gNoise.Load( int3(x_idx & 0xff, y_idx & 0xff, 0) );
			
			float height = gHgt.Load( int3( x>>1, y>>1, 0) );
			uint uHgt = (height - tileC.origin.y) / tileC.scale_1024;
			
			
			float FACTOR = 0.5f / tile.scale_1024 / 2.0f;		// FIXME we need plant sizes in teh GPU ecitipe desc
			
			if (FACTOR > 40.0)	
			{
				// *** its large pack into the plant structure
				uint slot = 0;
				InterlockedAdd( tiles[child_index].numPlants, 1, slot );
				
				plant_instance[child_index*2000 + slot].xyz =  repack_pos(x, y, uHgt, rnd);
				plant_instance[child_index*2000 + slot].s_r_idx = repack_SRTI( SRTI, child_index);
			}
			else
			{
				uint slot = 0;
				InterlockedAdd( tiles[child_index].numQuads, 1, slot );
				
				quad_instance[child_index * numQuadsPerTile + slot].xyz =  repack_pos(x, y, uHgt, rnd);
				quad_instance[child_index * numQuadsPerTile + slot].s_r_idx = repack_SRTI( SRTI, child_index);
			}
		}
	}
}
