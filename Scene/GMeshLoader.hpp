//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GSCENELOADER_HPP
#define GSGI_GSCENELOADER_HPP

#include "GMesh.hpp"
#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct GMeshLoader
{
    static ref<GMesh> load(const ref<Device>& pDevice, const std::filesystem::path& filename);
};

} // namespace GSGI

#endif // GSGI_GSCENELOADER_HPP
