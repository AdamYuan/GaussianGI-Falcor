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
#define SERIALIZER_REGISTER_POD(TYPE)                   \
    template<>                                          \
    struct Serializer<TYPE>                             \
    {                                                   \
        static void write(auto&& ostr, const TYPE& val) \
        {                                               \
            const char* src = (const char*)(&val);      \
            ostr.write(src, sizeof(TYPE));              \
        }                                               \
        static TYPE read(auto&& istr)                   \
        {                                               \
            TYPE val{};                                 \
            istr.read((char*)(&val), sizeof(TYPE));     \
            return val;                                 \
        }                                               \
    }

SERIALIZER_REGISTER_POD(bool);
SERIALIZER_REGISTER_POD(char);
SERIALIZER_REGISTER_POD(uint8_t);
SERIALIZER_REGISTER_POD(int8_t);
SERIALIZER_REGISTER_POD(uint16_t);
SERIALIZER_REGISTER_POD(int16_t);
SERIALIZER_REGISTER_POD(uint32_t);
SERIALIZER_REGISTER_POD(int32_t);
SERIALIZER_REGISTER_POD(uint64_t);
SERIALIZER_REGISTER_POD(int64_t);

template<typename T>
struct Serializer<std::vector<T>>
{
    static void write(auto&& ostr, const std::vector<T>& val)
    {
        Serializer<uint32_t>::write(ostr, val.size());
        for (const auto& i : val)
            Serializer<T>::write(ostr, i);
    }
    static void write(auto&& ostr, std::span<const T> val)
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

template<typename Version_T, typename... Ts>
struct SerializePersist
{
    inline static const char* const kIdentifier =
        typeid(std::pair<std::tuple<Version_T, Ts...>, std::index_sequence<sizeof(Version_T), sizeof(Ts)...>>).name();

    static void store(auto&& ostr, const auto& version, const auto&... vals)
    {
        if (!ostr)
            return;
        Serializer<std::string>::write(ostr, kIdentifier);
        Serializer<Version_T>::write(ostr, version);
        (Serializer<Ts>::write(ostr, vals), ...);
    }

    static bool load(auto&& istr, const auto& version, auto&... vals)
    {
        if (!istr)
            return false;
        if (Serializer<std::string>::read(istr) != kIdentifier)
            return false;
        if (Serializer<Version_T>::read(istr) != version)
            return false;
        ((vals = Serializer<Ts>::read(istr)), ...);
        return true;
    }
};

} // namespace GSGI

#endif // GSGI_SERIALIZEUTIL_HPP
