import MeshGSTrainer;

using namespace MeshGSTrainer;

uniform uint gSplatCount;
RWSplatBuffer gDLossDSplats;

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplatCount)
        return;
    gDLossDSplats.store(splatID, (Splat)0);
}
