//
// Created by adamyuan on 11/27/24.
//

#pragma once
#ifndef GSGI_GBOUND_HPP
#define GSGI_GBOUND_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct GBound
{
    float3 bMin{std::numeric_limits<float>::max()}, bMax{-std::numeric_limits<float>::max()};
    bool empty() const { return math::any(bMin > bMax); }
    float3 getExtent() const { return bMax - bMin; }
    float3 getCenter() const { return (bMin + bMax) * 0.5f; }
    void merge(const float3& p)
    {
        bMin = math::min(bMin, p);
        bMax = math::max(bMax, p);
    }
    void merge(const GBound& b)
    {
        bMin = math::min(bMin, b.bMin);
        bMax = math::max(bMax, b.bMax);
    }
};

} // namespace GSGI

#endif // GSGI_GBOUND_HPP
