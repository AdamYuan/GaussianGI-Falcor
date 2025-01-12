StructuredBuffer<uint> gSplatViewDrawArgs; // drawArgs[1] is instanceCount also splatViewCount
RWStructuredBuffer<uint> gSplatViewDispatchArgs;

#define DIM 64

[numthreads(1, 1, 1)]
void csMain()
{
    gSplatViewDispatchArgs[0] = (gSplatViewDrawArgs[1] + DIM - 1) / DIM;
}
