//
// Created by adamyuan on 12/11/24.
//

#ifndef GSGI_ENUMUTIL_HPP
#define GSGI_ENUMUTIL_HPP

#include <Falcor.h>
#include <concepts>

using namespace Falcor;

namespace GSGI
{

#define GSGI_ENUM_COUNT _Count

template<typename Enum_T, Enum_T Value>
struct EnumInfo
{};

#define GSGI_ENUM_REGISTER_TYPE(ENUM_T, VALUE, NAME, TYPE) \
    template<>                                             \
    struct EnumInfo<ENUM_T, ENUM_T::VALUE>                 \
    {                                                      \
        static constexpr ENUM_T kValue = ENUM_T::VALUE;    \
        static constexpr const char* kName = NAME;         \
        using Type = TYPE;                                 \
    }
#define GSGI_ENUM_REGISTER(ENUM_T, VALUE, NAME) GSGI_ENUM_REGISTER_TYPE(ENUM_T, VALUE, NAME, void)

template<typename Enum_T>
inline void enumForEach(auto&& func)
{
    [&]<size_t... Indices>(std::index_sequence<Indices...>)
    {
        (func(EnumInfo<Enum_T, static_cast<Enum_T>(Indices)>{}), ...);
    }(std::make_index_sequence<static_cast<std::size_t>(Enum_T::GSGI_ENUM_COUNT)>{});
}

template<typename Enum_T>
inline bool enumDropdown(Gui::Widgets& widget, const char label[], Enum_T& var, bool sameLine = false)
{
    static const auto kDropdownList = []
    {
        Gui::DropdownList l;
        enumForEach<Enum_T>([&]<typename EnumInfo_T>(EnumInfo_T)
                            { l.push_back({static_cast<uint32_t>(EnumInfo_T::kValue), EnumInfo_T::kName}); });
        return l;
    }();
    auto i = static_cast<uint32_t>(var);
    bool r = widget.dropdown(label, kDropdownList, i, sameLine);
    var = static_cast<Enum_T>(i);
    return r;
}

} // namespace GSGI

#endif // GSGI_ENUMUTIL_HPP
