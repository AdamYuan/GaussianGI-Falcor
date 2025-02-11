//
// Created by adamyuan on 11/27/24.
//

#pragma once
#ifndef GSGI_GTRANSFORM_HPP
#define GSGI_GTRANSFORM_HPP

#include <Falcor.h>
#include <Utils/Math/AABB.h>

using namespace Falcor;

namespace GSGI
{

struct GTransform
{
    float3 center{};
    float16_t scale{1.0f};
    float16_t rotateY{0.0f};

    float3 apply(float3 p) const
    {
        float cosRotY = math::cos(float{rotateY}), sinRotY = math::sin(float{rotateY});
        // [ cosR  -sinR ]
        // [ sinR   cosR ]
        float2 pXZ = {
            cosRotY * p.x - sinRotY * p.z,
            sinRotY * p.x + cosRotY * p.z,
        };
        p.x = pXZ[0];
        p.z = pXZ[1];
        p *= float{this->scale};
        p += this->center;
        return p;
    }
    AABB apply(AABB b) const
    {
        float cosRotY = math::cos(float{rotateY}), sinRotY = math::sin(float{rotateY});
        // [ cosR  -sinR ]
        // [ sinR   cosR ]
        float2 maxPointXZ{
            cosRotY * (cosRotY > 0 ? b.maxPoint.x : b.minPoint.x) - sinRotY * (sinRotY < 0 ? b.maxPoint.z : b.minPoint.z),
            sinRotY * (sinRotY > 0 ? b.maxPoint.x : b.minPoint.x) + cosRotY * (cosRotY > 0 ? b.maxPoint.z : b.minPoint.z),
        };
        float2 minPointXZ{
            cosRotY * (cosRotY < 0 ? b.maxPoint.x : b.minPoint.x) - sinRotY * (sinRotY > 0 ? b.maxPoint.z : b.minPoint.z),
            sinRotY * (sinRotY < 0 ? b.maxPoint.x : b.minPoint.x) + cosRotY * (cosRotY < 0 ? b.maxPoint.z : b.minPoint.z),
        };

        b.maxPoint.x = maxPointXZ[0];
        b.maxPoint.z = maxPointXZ[1];
        b.minPoint.x = minPointXZ[0];
        b.minPoint.z = minPointXZ[1];

        b.maxPoint *= float{this->scale};
        b.minPoint *= float{this->scale};

        b.maxPoint += this->center;
        b.minPoint += this->center;

        return b;
    }
    float4x4 getMatrix() const
    {
        float cosRotY = math::cos(float{rotateY}), sinRotY = math::sin(float{rotateY});
        float4x4 mat{};
        mat[0] = float4{scale * cosRotY, 0, -scale * sinRotY, center.x};
        mat[1] = float4{0, scale, 0, center.y};
        mat[2] = float4{scale * sinRotY, 0, scale * cosRotY, center.z};
        mat[3] = float4{0, 0, 0, 1};
        return mat;
    }
    float3x4 getMatrix3x4() const { return getMatrix(); }

    bool renderUI(Gui::Widgets& widget)
    {
        bool changed = false;
        if (widget.var("Center", center))
            changed = true;
        float fScale = scale;
        if (widget.var("Scale", fScale, 0.0f))
        {
            scale = float16_t{fScale};
            changed = true;
        }
        float fRotY = rotateY;
        if (widget.var("Y-Rotate", fRotY))
        {
            fRotY = std::remainder(fRotY, float{M_2PI});
            rotateY = float16_t{fRotY};
            changed = true;
        }
        return changed;
    }

    void bindShaderData(const ShaderVar& var) const
    {
        var["center"] = this->center;
        var["packedScaleRotateY"] = uint32_t{math::asuint16(this->scale)} | uint32_t{math::asuint16(this->rotateY)} << 16u;
    }
};

static_assert(sizeof(GTransform) == 4 * sizeof(uint32_t));

} // namespace GSGI

#endif // GSGI_GTRANSFORM_HPP
