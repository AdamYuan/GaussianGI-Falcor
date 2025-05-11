import Utils.Math.FormatConversion;
import Utils.Math.MathHelpers;
import GaussianGI.Algorithm.ThreadGroupTilingX;
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Renderer.GVBuffer;

uniform GVBuffer gGVBuffer;
RWTexture2D<uint2> gZNormals;

#define GROUP_DIM 8

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 coord = threadGroupTilingX((gGVBuffer.resolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(coord >= gGVBuffer.resolution))
        return;

    var hitInfo = gGVBuffer.LoadHitInfo(coord);

    if (hitInfo.isHit)
    {
        var hitPoint = gGStaticScene.getHitPoint(hitInfo);
        float4 clip = mul(gGStaticScene.camera.getViewProj(), float4(hitPoint.position, 1));
        float z = clip.z / clip.w;
        gZNormals[coord] = uint2(asuint(z), packUnorm2x16(ndir_to_oct_unorm(hitPoint.normal)));
    }
    /* else
    {
        gZNormals[coord] = -1;
    } */
}
