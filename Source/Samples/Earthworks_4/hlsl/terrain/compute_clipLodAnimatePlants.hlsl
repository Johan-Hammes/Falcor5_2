
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"	
#include "vegetation_defines.hlsli"


StructuredBuffer<gpuTile>           tiles;
StructuredBuffer<tileLookupStruct>  tileLookup;
StructuredBuffer<instance_PLANT>    plantBuffer;

RWStructuredBuffer<xformed_PLANT>   output;
RWStructuredBuffer<t_DrawArguments> drawArgs_Plants;
RWStructuredBuffer<GC_feedback>		feedback;

// new
StructuredBuffer<plant> plant_buffer;
RWStructuredBuffer<block_data> block_buffer;
RWStructuredBuffer<vegetation_feedback> feedback_Veg;

cbuffer gConstantBuffer
{
    float4x4 view;
    float4x4 clip;
};


//32 LIKELY BETETR
[numthreads(64, 1, 1)]			
void main(uint plantId : SV_GroupThreadID, uint blockId : SV_GroupID)
{
    const uint tileIDX = tileLookup[blockId].tile & 0xffff;
    const uint numQuad = tileLookup[blockId].tile >> 16;

    if (plantId < numQuad)
    {
        // clip
        instance_PLANT instance = plantBuffer[tileLookup[blockId].offset + plantId];
        const uint idx = PLANT_INDEX(instance.s_r_idx);
        const plant PLANT = plant_buffer[idx];
        float3 position = unpack_pos(instance.xyz, tiles[tileIDX].origin, tiles[tileIDX].scale_1024);
        float scale = SCALE(instance.s_r_idx);
        float4 viewBS = mul( float4(position, 1), view);
        float4 test = saturate( mul(clip, viewBS ) + 0.05 * float4(4, 4, 4, 4));
        bool inFrust = all(test);
        // FIXME move middle upwards, expand on thsi a bit, as well as teh 4 4 4 4 above

        // extract and save
        inFrust = false;
        if (inFrust)
        {
            float distance = length(viewBS.xyz); // can use view.z but that causes lod changes on rotation and is less good, although mnore acurate
            float pix = 3 * PLANT.size.y * scale / distance * 1080; // And add a user controlled scale in as well
            //lodBias

            int lod = 0;
            
            for (int i = 0; i <= PLANT.numLods; i++)
            {
                float size = PLANT.lods[i].pixSize;
                if (pix >= size)
                    lod = i;
            }


            //if (firstLod < 0 || lod == firstLod)
            {

                uint slot = 0;
                InterlockedAdd(feedback[0].numPostClippedPlants, 1, slot);
                //InterlockedAdd(feedback[0].numLod[lod + 1], 1, slot);
            
                if (idx == 0)
                {
                    //feedback[0].plantZero_pixeSize = pix;
                    //feedback[0].plantZeroLod = lod;
                }
            

            
                //InterlockedAdd(feedback[0].numBlocks, PLANT.lods[lod].numBlocks, slot);
                InterlockedAdd(drawArgs_Plants[0].instanceCount, PLANT.lods[lod].numBlocks, slot);
                drawArgs_Plants[0].vertexCountPerInstance = VEG_BLOCK_SIZE;
            
 
            
                for (int i = 0; i < PLANT.lods[lod].numBlocks; i++)
                {
                    block_buffer[slot + i].instance_idx = idx;  // Pretty sure this is wrong, and we need more data for rendering
                    block_buffer[slot + i].plant_idx = idx;
                    block_buffer[slot + i].section_idx = 0; // FIXME add later
                    block_buffer[slot + i].vertex_offset = PLANT.lods[lod].startVertex + (i * VEG_BLOCK_SIZE);
                }
            }
/*
old code
            uint slot = 0;
            InterlockedAdd(drawArgs_Plants[0].instanceCount, 1, slot);

            output[slot].position = position;
            output[slot].scale = 4 * scale;
            output[slot].rotation = ROTATIONxy(instance.s_r_idx);
            output[slot].index = PLANT_INDEX(instance.s_r_idx); // just write 1 function that fills it all

            InterlockedAdd(feedback[0].numPostClippedPlants, 1, slot);
*/
        }
    }

}
