#include "MeshGSTrainer.slangh"

StructuredBuffer<uint> gSplatViewDrawArgs; // drawArgs[1] is instanceCount also splatViewCount
RWStructuredBuffer<uint> gSplatViewDispatchArgs;

[numthreads(1, 1, 1)]
void csMain()
{
    gSplatViewDispatchArgs[0] = (gSplatViewDrawArgs[1] + BACKWARD_VIEW_DIM - 1) / BACKWARD_VIEW_DIM;
}
