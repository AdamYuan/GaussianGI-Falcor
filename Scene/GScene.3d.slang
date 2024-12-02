#include "GaussianGI/Scene/GMesh.slangh"
#include "Utils/Math/MathConstants.slangh"

import GaussianGI.Scene.GScene;

struct PSIn
{
    linear float2 texcoord : TEXCOORD;
    linear float3 normal : NORMAL;
    linear float3 worldPos : WORLD_POS;

    float4 clipPos : SV_POSITION;
}

PSIn vsMain(GMeshVSIn vIn, uint instanceID: SV_InstanceID)
{
    PSIn o;
    o.texcoord = vIn.texcoord;
    o.normal = vIn.normal;
    o.worldPos = gGScene.transform.apply(vIn.position);
    o.clipPos = mul(gGScene.camera.getViewProj(), float4(o.worldPos, 1));

    return o;
}

float4 psMain(PSIn pIn, uint primitiveID: SV_PrimitiveID)
{
    var texture = gGScene.getTexture(primitiveID);
    var normal = normalize(pIn.normal);
    if (dot(normal, gGScene.camera.getPosition() - pIn.worldPos) < 0)
        normal = -normal;
    float4 color = texture.Sample(gGScene.sampler, pIn.texcoord);
    if (color.a < 0.5)
        discard;

    float3 brdf = color.rgb * M_1_PI;
    float3 radiance = brdf * gGScene.lighting.evaluateIrradiance(normal);

    return float4(radiance, 1);
}
