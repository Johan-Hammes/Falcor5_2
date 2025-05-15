#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		
#include "vegetation_defines.hlsli"



RWStructuredBuffer<t_DrawArguments> DrawArgs_Quads;
RWStructuredBuffer<t_DrawArguments> DrawArgs_Plants;

StructuredBuffer<plant> plant_buffer;
StructuredBuffer<plant_instance> instance_buffer;

RWStructuredBuffer<block_data> block_buffer;
RWStructuredBuffer<plant_instance> instance_buffer_billboard;


cbuffer gConstantBuffer
{
    float4x4 view;
    float4x4 frustum;
};



// lets see AMD recommends 256, seems I have opick too small vlaues in tehpast - TEST
[numthreads(256, 1, 1)]
void main(uint idx : SV_DispatchThreadId)
{
    const plant_instance INSTANCE = instance_buffer[idx];
    const plant PLANT = plant_buffer[INSTANCE.plant_idx];

    
    float radius = PLANT.size.x + PLANT.size.y; // or something like that, or precalc radius
    float4 viewPos = mul(float4(INSTANCE.position, 1), view); //??? maak seker

    float4 test = saturate(mul(frustum, viewPos) + float4(radius, radius, radius, radius));
    
    if (all(test))
    {
        float distance = length(viewPos.xyz); // can use view.z but that causes lod changes on rotation and is less good, although mnore acurate
        float pix = 4.5 * PLANT.size.y * INSTANCE.scale / distance * 1080; // And add a user controlled scale in as well
        //float pix = 5 / distance * 1080; // And add a user controlled scale in as well
        //pix = 10;
        if (INSTANCE.position.x < 0)
        {
        
            //pix = 10;
        }
        //else
           //pix = 70;

        //if (pix > 64) pix = 300;

            if (pix < 64)
            {
            // do billboards
                uint slot = 0;
                InterlockedAdd(DrawArgs_Quads[0].vertexCountPerInstance, 1, slot);
                instance_buffer_billboard[slot] = INSTANCE;

            }
            else
            {
            // now the actual lodding info needs to be in the plant itself but hardcode for now
                int lod = PLANT.numLods - 1;
            
                for (int i = 0; i < PLANT.numLods; i++)
                {
                    if (pix < PLANT.lods[lod].pixSize)
                        lod = i;
                }

            //lod = 4;

                uint slot = 0;
                InterlockedAdd(DrawArgs_Plants[0].instanceCount, PLANT.lods[lod].numBlocks, slot);
                for (int i = 0; i < PLANT.lods[lod].numBlocks; i++)
                {
                    block_buffer[slot + i].instance_idx = idx;
                    block_buffer[slot + i].plant_idx = 0; //                INSTANCE.plant_idx;
                    block_buffer[slot + i].section_idx = 0; // FIXME add later
                    block_buffer[slot + i].vertex_offset = PLANT.lods[lod].startVertex + (i * VEG_BLOCK_SIZE);
                }
            }
    }
}
