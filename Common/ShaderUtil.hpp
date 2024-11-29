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
std::tuple<const ref<Program>&, const ShaderVar&> getShaderProgVar(const ref<Pass_T>& pass)
{
    return {pass->getProgram(), pass->getRootVar()};
}

} // namespace GSGI

#endif // GSGI_SHADERUTIL_HPP