//
// Created by adamyuan on 12/6/24.
//

#pragma once
#ifndef GSGI_TEXTUREUTIL_HPP
#define GSGI_TEXTUREUTIL_HPP

#include <Falcor.h>
#include <concepts>

using namespace Falcor;

namespace GSGI
{

template<typename Texture_T> // Texture_T can be Texture or Fbo
inline uint2 getTextureResolution2(const ref<Texture_T>& pTexture)
{
    if (pTexture == nullptr)
        return {};
    return {pTexture->getWidth(), pTexture->getHeight()};
}

inline uint3 getTextureResolution3(const ref<Texture>& pTexture)
{
    if (pTexture == nullptr)
        return {};
    return {pTexture->getWidth(), pTexture->getHeight(), pTexture->getDepth()};
}

template<typename Texture_T> // Texture_T can be Texture or Fbo
inline bool updateTextureSize(ref<Texture_T>& pTexture, uint2 resolution, std::invocable<uint, uint> auto&& createTexture)
{
    if (!pTexture || math::any(resolution != getTextureResolution2(pTexture)))
    {
        pTexture = createTexture(resolution.x, resolution.y);
        return true;
    }
    return false;
}

inline bool updateTextureSize(ref<Texture>& pTexture, uint3 resolution, std::invocable<uint, uint, uint> auto&& createTexture)
{
    if (!pTexture || math::any(resolution != getTextureResolution3(pTexture)))
    {
        pTexture = createTexture(resolution.x, resolution.y, resolution.z);
        return true;
    }
    return false;
}

inline ref<Texture> createColorTexture(const ref<Device>& pDevice, const uint8_t rgba[4], ResourceBindFlags bindFlags)
{
    return pDevice->createTexture2D(1, 1, ResourceFormat::RGBA8Unorm, 1, 1, rgba, bindFlags);
}

} // namespace GSGI

#endif // GSGI_TEXTUREUTIL_HPP