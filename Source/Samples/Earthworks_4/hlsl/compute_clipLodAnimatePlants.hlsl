
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"	



StructuredBuffer<gpuTile>           tiles;
StructuredBuffer<tileLookupStruct>  tileLookup;
StructuredBuffer<instance_PLANT>    plantBuffer;

RWStructuredBuffer<xformed_PLANT>   output;
RWStructuredBuffer<t_DrawArguments> drawArgs_Plants;
RWStructuredBuffer<GC_feedback>		feedback;


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
        instance_PLANT plant = plantBuffer[tileLookup[blockId].offset + plantId];
        float3 position = unpack_pos(plant.xyz, tiles[tileIDX].origin, tiles[tileIDX].scale_1024);
        float scale = SCALE(plant.s_r_idx);
        float4 viewBS = mul( float4(position, 1), view);
        float4 test = saturate( mul(clip, viewBS ) + float4(4, 4, 4, 4));
        bool inFrust = all(test);

        // extract and save
        if (inFrust)
        {
            uint slot = 0;
            InterlockedAdd(drawArgs_Plants[0].instanceCount, 1, slot);

            output[slot].position = position;
            output[slot].scale = 4 * scale;
            output[slot].rotation = ROTATIONxy(plant.s_r_idx);
            output[slot].index = PLANT_INDEX(plant.s_r_idx);        // just write 1 function that fills it all

            InterlockedAdd(feedback[0].numPostClippedPlants, 1, slot);
        }
    }

}
