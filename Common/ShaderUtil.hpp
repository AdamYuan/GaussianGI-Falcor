//
// Created by adamyuan on 11/29/24.
//

#pragma once
#ifndef GSGI_SHADERUTIL_HPP
#define GSGI_SHADERUTIL_HPP

#include <Falcor.h>
#include <tuple>

using namespace Falcor;

namespace GSGI
{

template<typename Pass_T>
inline std::tuple<ref<Program>, ShaderVar> getShaderProgVar(const ref<Pass_T>& pass)
{
    return std::make_tuple(pass->getProgram(), pass->getRootVar());
}

} // namespace GSGI

#endif // GSGI_SHADERUTIL_HPP