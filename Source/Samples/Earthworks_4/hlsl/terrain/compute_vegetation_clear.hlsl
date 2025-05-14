#include "vegetation_defines.hlsli"
#include "groundcover_defines.hlsli"


RWStructuredBuffer<t_DrawArguments> DrawArgs_Quads;
RWStructuredBuffer<t_DrawArguments> DrawArgs_Plants;




// lets see AMD recommends 256, seems I have opick too small vlaues in tehpast - TEST
[numthreads(1, 1, 1)]
void main(uint idx : SV_DispatchThreadId)
{
    DrawArgs_Plants[0].instanceCount = 0;
    DrawArgs_Plants[0].vertexCountPerInstance = VEG_BLOCK_SIZE;
    DrawArgs_Plants[0].startInstanceLocation = 0;
    DrawArgs_Plants[0].startVertexLocation = 0;

    DrawArgs_Quads[0].instanceCount = 1;
    DrawArgs_Quads[0].vertexCountPerInstance = 0;
    DrawArgs_Quads[0].startInstanceLocation = 0;
    DrawArgs_Quads[0].startVertexLocation = 0;
}
