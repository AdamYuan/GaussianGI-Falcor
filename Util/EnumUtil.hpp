//
// Created by adamyuan on 12/11/24.
//

#ifndef GSGI_ENUMUTIL_HPP
#define GSGI_ENUMUTIL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

#define GSGI_ENUM_COUNT _Count

template<auto Enum_V>
struct EnumInfo
{};

#define GSGI_ENUM_REGISTER(ENUM_V, TYPE, LABEL, PROPERTY_T, ...) \
    template<>                                                   \
    struct EnumInfo<ENUM_V>                                      \
    {                                                            \
        using Enum = decltype(ENUM_V);                           \
        static constexpr Enum kValue = ENUM_V;                   \
        using Type = TYPE;                                       \
        static constexpr const char* kLabel = LABEL;             \
        static constexpr PROPERTY_T kProperty = {__VA_ARGS__};   \
    }

template<typename Enum_T>
inline constexpr std::size_t kEnumCount = static_cast<std::size_t>(Enum_T::GSGI_ENUM_COUNT);

template<typename Enum_T>
using EnumIndexSequence = std::make_index_sequence<kEnumCount<Enum_T>>;

template<typename Enum_T>
inline void enumForEach(auto&& func)
{
    [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
    { (func(EnumInfo<static_cast<Enum_T>(Indices)>{}), ...); }(EnumIndexSequence<Enum_T>{});
}

template<typename Enum_T>
inline auto enumVisit(Enum_T value, auto&& func)
{
    static_assert(kEnumCount<Enum_T> != 0);
    const auto visitFuncImpl = [&]<std::size_t Index>(auto&& visitFunc)
    {
        if (value == static_cast<Enum_T>(Index))
            return func(EnumInfo<static_cast<Enum_T>(Index)>{});

        if constexpr (Index < kEnumCount<Enum_T> - 1)
            return visitFunc.template operator()<Index + 1>(visitFunc);
        else
            FALCOR_CHECK(false, "Invalid enum value");
    };
    return visitFuncImpl.template operator()<0>(visitFuncImpl);
}

template<typename Enum_T>
inline bool enumDropdown(Gui::Widgets& widget, const char label[], Enum_T& var, bool sameLine = false)
{
    static const auto kDropdownList = []
    {
        Gui::DropdownList l;
        enumForEach<Enum_T>([&]<typename EnumInfo_T>(EnumInfo_T)
                            { l.push_back({static_cast<uint32_t>(EnumInfo_T::kValue), EnumInfo_T::kLabel}); });
        return l;
    }();
    auto i = static_cast<uint32_t>(var);
    bool r = widget.dropdown(label, kDropdownList, i, sameLine);
    var = static_cast<Enum_T>(i);
    return r;
}

// Enum Wrap
template<typename Enum_T, template<auto Enum_V> typename ValueType_T, template<typename...> typename Wrapper_T, typename>
struct EnumWrapper;
template<typename Enum_T, template<auto Enum_V> typename ValueType_T, template<typename...> typename Wrapper_T, std::size_t... Indices>
struct EnumWrapper<Enum_T, ValueType_T, Wrapper_T, std::index_sequence<Indices...>>
{
    using Type = Wrapper_T<ValueType_T<static_cast<Enum_T>(Indices)>...>;
};
template<typename Enum_T, template<auto Enum_V> typename ValueType_T, template<typename...> typename Wrapper_T>
using EnumWrap = typename EnumWrapper<Enum_T, ValueType_T, Wrapper_T, EnumIndexSequence<Enum_T>>::Type;

// Enum Tuple
template<auto Enum_V>
using EnumType = typename EnumInfo<Enum_V>::Type;
template<auto Enum_V>
using EnumRefType = ref<EnumType<Enum_V>>;
template<typename Enum_T>
using EnumTuple = EnumWrap<Enum_T, EnumType, std::tuple>;
template<typename Enum_T>
using EnumRefTuple = EnumWrap<Enum_T, EnumRefType, std::tuple>;
template<auto Enum_V, typename... Tuple_Ts>
inline const auto& enumTupleGet(const std::tuple<Tuple_Ts...>& t)
{
    static_assert(sizeof...(Tuple_Ts) == kEnumCount<decltype(Enum_V)>);
    return std::get<static_cast<std::size_t>(Enum_V)>(t);
}
template<auto Enum_V, typename... Tuple_Ts>
inline auto& enumTupleGet(std::tuple<Tuple_Ts...>& t)
{
    static_assert(sizeof...(Tuple_Ts) == kEnumCount<decltype(Enum_V)>);
    return std::get<static_cast<std::size_t>(Enum_V)>(t);
}

// Enum Bitset
template<typename Enum_T>
using EnumBitset = std::bitset<kEnumCount<Enum_T>>;

template<typename Enum_T>
inline void enumBitsetSet(EnumBitset<Enum_T>& bitset, Enum_T key, bool bit = true)
{
    bitset.set(static_cast<std::size_t>(key), bit);
}

template<typename Enum_T>
inline bool enumBitsetTest(const EnumBitset<Enum_T>& bitset, Enum_T key)
{
    return bitset.test(static_cast<std::size_t>(key));
}

template<typename Enum_T, typename... Args>
inline EnumBitset<Enum_T> enumBitsetMake(Enum_T firstKey, Args... otherKeys)
{
    EnumBitset<Enum_T> bitset{};
    enumBitsetSet(bitset, firstKey);
    (enumBitsetSet<Enum_T>(bitset, otherKeys), ...);
    return bitset;
}

} // namespace GSGI

#endif // GSGI_ENUMUTIL_HPP
