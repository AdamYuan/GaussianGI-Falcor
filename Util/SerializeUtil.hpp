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
concept StandardLayout = std::is_standard_layout_v<T>;
}

template<typename>
struct Serializer;

// Ignore big/little-endian for simplicity
template<Concepts::StandardLayout T>
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

template<typename T>
struct Serializer<std::vector<T>>
{
    static void write(auto&& ostr, const std::vector<T>& val)
    {
        Serializer<uint32_t>::write(ostr, val.size());
        for (const auto& i : val)
            Serializer<T>::write(ostr, i);
    }
    static std::vector<T> read(auto&& istr)
    {
        uint32_t size = Serializer<uint32_t>::read(istr);
        std::vector<T> ret;
        ret.reserve(size);
        while (size--) // Might cause problem
            ret.push_back(Serializer<T>::read(istr));
        return ret;
    }
};

template<>
struct Serializer<std::string>
{
    static void write(auto&& ostr, std::string_view val)
    {
        Serializer<uint32_t>::write(ostr, val.size());
        for (const auto& i : val)
            Serializer<char>::write(ostr, i);
    }
    static std::string read(auto&& istr)
    {
        uint32_t size = Serializer<uint32_t>::read(istr);
        std::string ret;
        ret.reserve(size);
        while (size--) // Might cause problem
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

struct SerializePersist
{
    template<typename Version_T, typename... Ts>
    static void store(auto&& ostr, const Version_T& version, const Ts&... vals)
    {
        if (!ostr)
            return;
        Serializer<std::string>::write(ostr, typeid(std::tuple<Version_T, Ts...>).name());
        Serializer<std::size_t>::write(ostr, sizeof(Version_T));
        (Serializer<std::size_t>::write(ostr, sizeof(Ts)), ...);
        Serializer<Version_T>::write(ostr, version);
        (Serializer<Ts>::write(ostr, vals), ...);
    }

    template<typename Version_T, typename... Ts>
    static bool load(auto&& istr, const Version_T& version, Ts&... vals)
    {
        if (!istr)
            return false;
        if (Serializer<std::string>::read(istr) != typeid(std::tuple<Version_T, Ts...>).name())
            return false;
        if (Serializer<std::size_t>::read(istr) != sizeof(Version_T) || ((Serializer<std::size_t>::read(istr) != sizeof(Ts)) || ...))
            return false;
        if (Serializer<Version_T>::read(istr) != version)
            return false;
        ((vals = Serializer<Ts>::read(istr)), ...);
        return true;
    }
};

} // namespace GSGI

#endif // GSGI_SERIALIZEUTIL_HPP
