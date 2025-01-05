//
// Created by adamyuan on 1/5/25.
//

#ifndef GSGI_SERIALIZEUTIL_HPP
#define GSGI_SERIALIZEUTIL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

namespace Concepts
{
template<typename T>
concept POD = std::is_trivial_v<T> && std::is_standard_layout_v<T>;
}

template<typename>
struct Serializer;

// Ignore big/little-endian for simplicity
template<Concepts::POD T>
struct Serializer<T>
{
    static void write(auto&& ostr, const T& val)
    {
        const char* src = (const char*)(&val);
        ostr.write(src, sizeof(T));
    }
    static T read(auto&& istr)
    {
        T val;
        istr.read((char*)(&val), sizeof(T));
        return val;
    }
};

template<typename T, std::size_t S>
struct Serializer<std::array<T, S>>
{
    static void write(auto&& ostr, const std::array<T, S>& val)
    {
        for (std::size_t i = 0; i < S; ++i)
            Serializer<T>::write(ostr, val[i]);
    }
    static std::array<T, S> read(auto&& istr)
    {
        std::array<T, S> ret;
        for (std::size_t i = 0; i < S; ++i)
            ret[i] = Serializer<T>::read(istr);
        return ret;
    }
};

template<typename T>
struct Serializer<std::vector<T>>
{
    static void write(auto&& ostr, const std::vector<T>& val)
    {
        Serializer<std::size_t>::write(ostr, val.size());
        for (const auto& i : val)
            Serializer<T>::write(ostr, i);
    }
    static std::vector<T> read(auto&& istr)
    {
        std::size_t size = Serializer<std::size_t>::read(istr);
        std::vector<T> ret;
        ret.reserve(size);
        while (size--)
            ret.push_back(Serializer<T>::read(istr));
        return ret;
    }
};

template<>
struct Serializer<std::string>
{
    static void write(auto&& ostr, std::string_view val)
    {
        Serializer<std::size_t>::write(ostr, val.size());
        for (const auto& i : val)
            Serializer<char>::write(ostr, i);
    }
    static std::string read(auto&& istr)
    {
        std::size_t size = Serializer<std::size_t>::read(istr);
        std::string ret;
        ret.reserve(size);
        while (size--)
            ret.push_back(Serializer<char>::read(istr));
        return ret;
    }
};

template<typename T>
struct Serializer<std::optional<T>>
{
    static void write(auto&& ostr, const std::optional<T>& val)
    {
        Serializer<bool>::write(ostr, val.has_value());
        if (val.has_value())
            Serializer<T>::write(ostr, val.value());
    }
    static std::optional<T> read(auto&& istr)
    {
        bool has_value = Serializer<bool>::read(istr);
        if (has_value)
            return std::optional<T>{Serializer<T>::read(istr)};
        return std::optional<T>{};
    }
};

template<typename... Ts>
struct Serializer<std::tuple<Ts...>>
{
    static void write(auto&& ostr, const std::tuple<Ts...>& val)
    {
        std::apply([&ostr](const Ts&... args) { (Serializer<Ts>::write(ostr, args), ...); }, val);
    }
    static std::tuple<Ts...> read(auto&& istr)
    {
        std::tuple<Ts...> val;
        std::apply([&istr](Ts&... args) { ((args = Serializer<Ts>::read(istr)), ...); }, val);
        return val;
    }
};

} // namespace GSGI

#endif // GSGI_SERIALIZEUTIL_HPP
