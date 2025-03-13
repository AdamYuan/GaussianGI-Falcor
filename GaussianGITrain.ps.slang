import Algorithm.MeshGSTrainer.MeshGSTrainer;

using namespace MeshGSTrain;

SplatTexture gSplatRT;
Trait::MeshRTTexture gMeshRT;

uniform uint gDrawMesh;

#ifndef FIELD_NAME
#define FIELD_NAME channel.depth
#endif

float4 main(float4 fragPos: SV_POSITION)
{
    uint2 coord = uint2(fragPos.xy);

    SplatChannelT channelT;
    if (bool(gDrawMesh))
        channelT = gMeshRT.load(coord);
    else
        channelT = gSplatRT.load<SplatChannelT>(coord);

    return float4(float3(channelT.FIELD_NAME), 1);
}
