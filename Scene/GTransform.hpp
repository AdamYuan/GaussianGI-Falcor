//
// Created by adamyuan on 11/27/24.
//

#pragma once
#ifndef GSGI_GTRANSFORM_HPP
#define GSGI_GTRANSFORM_HPP

#include <Falcor.h>
using namespace Falcor;

#include "GBound.hpp"

namespace GSGI
{

struct GTransform
{
    float3 center{};
    float scale{1.0f};
    float cosRotY{1.0f}, sinRotY{0.0f};

    void normalizeRotY()
    {
        float invLength = 1.0f / math::length(float2(cosRotY, sinRotY));
        cosRotY *= invLength;
        sinRotY *= invLength;
    }
    float getRotYAngle() const { return math::atan2(sinRotY, cosRotY); }
    void setRotYAngle(float a)
    {
        cosRotY = math::cos(a);
        sinRotY = math::sin(a);
    }
    float3 apply(float3 p) const
    {
        // [ cosR  -sinR ]
        // [ sinR   cosR ]
        float2 pXZ = {
            cosRotY * p.x - sinRotY * p.z,
            sinRotY * p.x + cosRotY * p.z,
        };
        p.x = pXZ[0];
        p.z = pXZ[1];
        p *= this->scale;
        p += center;
        return p;
    }
    GBound apply(GBound b) const
    {
        // [ cosR  -sinR ]
        // [ sinR   cosR ]
        float2 bMaxXZ{
            cosRotY * (cosRotY > 0 ? b.bMax.x : b.bMin.x) - sinRotY * (sinRotY < 0 ? b.bMax.z : b.bMin.z),
            sinRotY * (sinRotY > 0 ? b.bMax.x : b.bMin.x) + cosRotY * (cosRotY > 0 ? b.bMax.z : b.bMin.z),
        };
        float2 bMinXZ{
            cosRotY * (cosRotY < 0 ? b.bMax.x : b.bMin.x) - sinRotY * (sinRotY > 0 ? b.bMax.z : b.bMin.z),
            sinRotY * (sinRotY < 0 ? b.bMax.x : b.bMin.x) + cosRotY * (cosRotY < 0 ? b.bMax.z : b.bMin.z),
        };

        b.bMax.x = bMaxXZ[0];
        b.bMax.z = bMaxXZ[1];
        b.bMin.x = bMinXZ[0];
        b.bMin.z = bMinXZ[1];

        b.bMax *= this->scale;
        b.bMin *= this->scale;

        b.bMax += center;
        b.bMin += center;

        return b;
    }

    void renderUI(Gui::Widgets& widget)
    {
        widget.var("Center", center);
        widget.var("Scale", scale, 0.0f);
        float rotY = getRotYAngle();
        if (widget.var("Y-Rotate", rotY))
            setRotYAngle(rotY);
    }
};

} // namespace GSGI

#endif // GSGI_GTRANSFORM_HPP
