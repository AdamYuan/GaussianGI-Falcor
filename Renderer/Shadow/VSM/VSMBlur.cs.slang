#include "GaussianGI/Algorithm/GaussianBlurKernel.slangh"
import GaussianGI.Algorithm.ThreadGroupTilingX;

#ifndef MAX_BLUR_RADIUS
#define MAX_BLUR_RADIUS 17
#endif

#ifndef BLUR_AXIS
#define BLUR_AXIS 0
#endif

uniform float gWeights[GAUSSBLUR_KERNEL_WEIGHT_SIZE(MAX_BLUR_RADIUS)];
uniform uint gRadius;
uniform uint2 gResolution;

Texture2D<float2> gSrc;
RWTexture2D<float2> gDst;

#define GROUP_DIM 8

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 coord = threadGroupTilingX((gResolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(coord >= gResolution))
        return;

    float2 color = float2(0);
    for (int offset = GAUSSBLUR_KERNEL_MIN_OFFSET(gRadius); offset <= GAUSSBLUR_KERNEL_MAX_OFFSET(gRadius); ++offset)
    {
        int2 neiCoord = coord;
        neiCoord[BLUR_AXIS] += offset;
        neiCoord = clamp(neiCoord, int2(0), int2(gResolution) - 1);
        color += gWeights[GAUSSBLUR_KERNEL_WEIGHT_IDX(offset)] * gSrc[neiCoord];
    }

    gDst[coord] = color;
}
