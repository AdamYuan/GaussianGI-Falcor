//
// Created by adamyuan on 11/24/24.
//

#pragma once
#ifndef GSGI_DEVICEOBJECTBASE_HPP
#define GSGI_DEVICEOBJECTBASE_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

class GObject : public Object
{
private:
    ref<Device> mpDevice;

public:
    explicit GObject(ref<Device> pDevice) : mpDevice{std::move(pDevice)} {}
    virtual ~GObject() = default;

    const ref<Device>& getDevice() const { return mpDevice; }
};

} // namespace GSGI

#endif
