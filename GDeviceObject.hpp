//
// Created by adamyuan on 11/24/24.
//

#pragma once
#ifndef GSGI_DEVICEOBJECTBASE_HPP
#define GSGI_DEVICEOBJECTBASE_HPP

#include <../Falcor/Falcor.h>

using namespace Falcor;

namespace GSGI
{

template<typename Derived_T>
class GDeviceObject : public Object
{
private:
    ref<Device> mpDevice;

public:
    explicit GDeviceObject(ref<Device> pDevice) : mpDevice{std::move(pDevice)} {}
    ~GDeviceObject() override = default;

    const ref<Device>& getDevice() const { return mpDevice; }

    template<typename... Args>
    void renderUI(Gui::Widgets& widget, Args&&... args)
    {
        ImGui::PushID(this);
        static_cast<Derived_T*>(this)->renderUIImpl(widget, std::forward<Args>(args)...);
        ImGui::PopID();
    }
};

} // namespace GSGI

#endif
