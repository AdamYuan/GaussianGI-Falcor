//
// Created by adamyuan on 12/20/24.
//

#include "../Algorithm/DeviceSort/DeviceSorter.hpp"
#include "Testing/UnitTest.h"
#include <random>

using namespace Falcor;

namespace GSGI
{

namespace
{

void printDesc(const char* name, const DeviceSortDesc& desc)
{
    fmt::println("DeviceSortDesc {}", name);
    fmt::println("Buffer Count: {}", desc.getBufferCount());
    for (uint32_t keyID = 0; keyID < desc.getKeyBufferCount(); ++keyID)
    {
        const auto& keyDesc = desc.getKeyBufferDesc(keyID);
        fmt::println(
            "Key {}: bufferID = {}, bitWidth = {}, isPayload = {}, payloadBufferIDs = [{}]",
            keyID,
            keyDesc.bufferID,
            keyDesc.bitWidth,
            keyDesc.isPayload,
            fmt::join(keyDesc.payloadBufferIDs, ", ")
        );
    }
    for (uint32_t passID = 0; passID < desc.getPassCount(); ++passID)
    {
        const auto& passDesc = desc.getPassDesc(passID);
        fmt::println(
            "Pass {}: keyID = {}, keyRadixShift = {}, keyWrite = {}", passID, passDesc.keyID, passDesc.keyRadixShift, passDesc.keyWrite
        );
    }
}

std::mt19937 rand{std::random_device{}()};

template<DeviceSortDispatchType DispatchType_V, DeviceSortBufferType... BufferType_Vs>
struct Config
{};

template<DeviceSortBufferType... BufferType_Vs>
using CpuItem = std::array<uint32_t, sizeof...(BufferType_Vs)>;

template<DeviceSortBufferType... BufferType_Vs>
using CpuBuffer = std::vector<CpuItem<BufferType_Vs...>>;

template<DeviceSortBufferType BufferType_V, uint32_t Index_V>
std::strong_ordering compareSingle(const auto& l, const auto& r)
{
    if constexpr (DeviceSortBufferTypeMethod::isKey(BufferType_V))
    {
        static constexpr auto kKeyBitWidth = DeviceSortBufferTypeMethod::getKeyBitWidth(BufferType_V);
        uint32_t keyL = l[Index_V], keyR = r[Index_V];
        if constexpr (kKeyBitWidth < 32)
        {
            static constexpr auto kKeyBitMask = (1u << kKeyBitWidth) - 1;
            keyL &= kKeyBitMask;
            keyR &= kKeyBitMask;
        }
        return std::strong_ordering{keyL <=> keyR};
    }
    else
        return std::strong_ordering::equal;
}

template<uint32_t Index_V, DeviceSortBufferType FirstBufferType_V, DeviceSortBufferType... OtherBufferType_Vs>
std::strong_ordering compare(const auto& l, const auto& r)
{
    if constexpr (sizeof...(OtherBufferType_Vs) > 0)
    {
        std::strong_ordering order = compare<Index_V + 1, OtherBufferType_Vs...>(l, r);
        if (order != 0)
            return order;
    }

    if constexpr (DeviceSortBufferTypeMethod::isKey(FirstBufferType_V))
    {
        static constexpr auto kKeyBitWidth = DeviceSortBufferTypeMethod::getKeyBitWidth(FirstBufferType_V);
        uint32_t keyL = l[Index_V], keyR = r[Index_V];
        if constexpr (kKeyBitWidth < 32)
        {
            static constexpr auto kKeyBitMask = (1u << kKeyBitWidth) - 1;
            keyL &= kKeyBitMask;
            keyR &= kKeyBitMask;
        }
        return std::strong_ordering{keyL <=> keyR};
    }
    else
        return std::strong_ordering::equal;
}

template<uint32_t Index_V, DeviceSortBufferType FirstBufferType_V, DeviceSortBufferType... OtherBufferType_Vs>
void validate(GPUUnitTestContext& ctx, const auto& cpuBuffer, const std::vector<ref<Buffer>>& pGpuBuffers, uint sortCount)
{
    if constexpr (DeviceSortBufferTypeMethod::isPayload(FirstBufferType_V))
    {
        auto gpuKeys = pGpuBuffers[Index_V]->getElements<uint32_t>(0, sortCount);
        for (uint32_t i = 0; i < sortCount; ++i)
            EXPECT_EQ_MSG(cpuBuffer[i][Index_V], gpuKeys[i], fmt::format("Index_V = {}, i = {}", Index_V, i));
    }

    if constexpr (sizeof...(OtherBufferType_Vs) > 0)
        validate<Index_V + 1, OtherBufferType_Vs...>(ctx, cpuBuffer, pGpuBuffers, sortCount);
}

template<DeviceSortDispatchType DispatchType_V, DeviceSortBufferType... BufferType_Vs>
std::tuple<DeviceSorter<DispatchType_V>, DeviceSortResource<DispatchType_V>> makeSorter(
    Config<DispatchType_V, BufferType_Vs...>,
    const ref<Device>& pDevice,
    uint count
)
{
    auto desc = DeviceSortDesc({BufferType_Vs...});
    printDesc(typeid(Config<DispatchType_V, BufferType_Vs...>).name(), desc);
    auto sorter = DeviceSorter<DispatchType_V>(pDevice, desc);
    auto sortResource = DeviceSortResource<DispatchType_V>::create(pDevice, desc, count);
    return {std::move(sorter), std::move(sortResource)};
}

template<DeviceSortDispatchType DispatchType_V, DeviceSortBufferType... BufferType_Vs>
std::tuple<CpuBuffer<BufferType_Vs...>, std::vector<ref<Buffer>>> makeBuffers(
    Config<DispatchType_V, BufferType_Vs...>,
    const ref<Device>& pDevice,
    uint32_t count,
    uint32_t maxValue = std::numeric_limits<uint32_t>::max()
)
{
    CpuBuffer<BufferType_Vs...> cpuBuffer(count);
    std::vector<ref<Buffer>> pGpuBuffers(sizeof...(BufferType_Vs));

    for (uint32_t i = 0; i < sizeof...(BufferType_Vs); ++i)
    {
        std::vector<uint32_t> keys(count);
        for (uint32_t j = 0; j < count; ++j)
            cpuBuffer[j][i] = keys[j] = std::uniform_int_distribution<uint32_t>{0, maxValue}(rand);

        pGpuBuffers[i] = pDevice->createStructuredBuffer(
            sizeof(uint32_t),
            count,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
            MemoryType::DeviceLocal,
            keys.data()
        );
    }
    return {std::move(cpuBuffer), std::move(pGpuBuffers)};
}

std::tuple<ref<Buffer>, uint64_t> makeCountBuffer(const ref<Device>& pDevice, uint count)
{
    uint elementCount = std::uniform_int_distribution<uint32_t>{16, 256}(rand);
    uint offset = std::uniform_int_distribution<uint32_t>{0, elementCount - 1}(rand);
    auto pBuffer =
        pDevice->createStructuredBuffer(sizeof(uint32_t), elementCount, ResourceBindFlags::ShaderResource, MemoryType::DeviceLocal);
    pBuffer->setElement<uint32_t>(offset, count);
    return {std::move(pBuffer), uint64_t(offset) * sizeof(uint32_t)};
}

template<DeviceSortDispatchType DispatchType, DeviceSortBufferType... BufferType_Vs>
void runTest(
    Config<DispatchType, BufferType_Vs...>,
    GPUUnitTestContext& ctx,
    const DeviceSorter<DispatchType>& sorter,
    const DeviceSortResource<DispatchType>& sortResource,
    CpuBuffer<BufferType_Vs...>& cpuBuffer,
    const std::vector<ref<Buffer>>& pGpuBuffers,
    uint32_t sortCount
)
{
    std::stable_sort(
        cpuBuffer.begin(),
        cpuBuffer.begin() + sortCount,
        [](const auto& l, const auto& r) { return compare<0, BufferType_Vs...>(l, r) < 0; }
    );
    if constexpr (DispatchType == DeviceSortDispatchType::kDirect)
        sorter.dispatch(ctx.getRenderContext(), pGpuBuffers, sortCount, sortResource);
    else
    {
        auto [pCountBuffer, countBufferOffset] = makeCountBuffer(ctx.getDevice(), sortCount);
        sorter.dispatch(ctx.getRenderContext(), pGpuBuffers, pCountBuffer, countBufferOffset, sortResource);
    }
    validate<0, BufferType_Vs...>(ctx, cpuBuffer, pGpuBuffers, sortCount);
}

} // namespace

CPU_TEST(DeviceSorter_PrintDesc)
{
    printDesc("PayloadKey32", DeviceSortDesc{{DeviceSortBufferType::kPayloadKey32}});
    printDesc("PayloadKey32 + Payload32", DeviceSortDesc{{DeviceSortBufferType::kPayloadKey32, DeviceSortBufferType::kPayload}});
    printDesc("Key32 + Payload", DeviceSortDesc{{DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}});
    printDesc(
        "Key32 + Key32 + Payload",
        DeviceSortDesc{{DeviceSortBufferType::kKey32, DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}}
    );
    printDesc(
        "Key32 + Key32Payload + Payload",
        DeviceSortDesc{{DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayloadKey32, DeviceSortBufferType::kPayload}}
    );
    printDesc(
        "PayloadKey32 + Key32 + Payload",
        DeviceSortDesc{{DeviceSortBufferType::kPayloadKey32, DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}}
    );
    printDesc(
        "Key32 + Payload + PayloadKey16",
        DeviceSortDesc{{DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload, DeviceSortBufferType::kPayloadKey16}}
    );
}

GPU_TEST(DeviceSorter_Key_Direct)
{
    Config<DeviceSortDispatchType::kDirect, DeviceSortBufferType::kPayloadKey32> config;
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto [sorter, sortResource] = makeSorter(config, ctx.getDevice(), count);
    auto [cpuBuffer, pGpuBuffers] = makeBuffers(config, ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    runTest(config, ctx, sorter, sortResource, cpuBuffer, pGpuBuffers, count);
}

GPU_TEST(DeviceSorter_Pair_Direct)
{
    Config<DeviceSortDispatchType::kDirect, DeviceSortBufferType::kPayloadKey32, DeviceSortBufferType::kPayload> config;
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto [sorter, sortResource] = makeSorter(config, ctx.getDevice(), count);
    auto [cpuBuffer, pGpuBuffers] = makeBuffers(config, ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    runTest(config, ctx, sorter, sortResource, cpuBuffer, pGpuBuffers, count);
}

GPU_TEST(DeviceSorter_Pair_IsStableSort)
{
    Config<DeviceSortDispatchType::kDirect, DeviceSortBufferType::kPayloadKey32, DeviceSortBufferType::kPayload> config;
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto [sorter, sortResource] = makeSorter(config, ctx.getDevice(), count);
    auto [cpuBuffer, pGpuBuffers] = makeBuffers(config, ctx.getDevice(), count, 16);
    fmt::println("Count = {}", count);
    runTest(config, ctx, sorter, sortResource, cpuBuffer, pGpuBuffers, count);
}

GPU_TEST(DeviceSorter_Key_Indirect)
{
    Config<DeviceSortDispatchType::kIndirect, DeviceSortBufferType::kPayloadKey32> config;

    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto [sorter, sortResource] = makeSorter(config, ctx.getDevice(), count);
    auto [cpuBuffer, pGpuBuffers] = makeBuffers(config, ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    for (uint i = 0, sortCount = 0; i < 5; ++i)
    {
        sortCount = std::uniform_int_distribution<uint>{sortCount, count}(rand);
        fmt::println("Sort Count = {}", sortCount);
        runTest(config, ctx, sorter, sortResource, cpuBuffer, pGpuBuffers, sortCount);
    }
}

GPU_TEST(DeviceSorter_Pair_Indirect)
{
    Config<DeviceSortDispatchType::kIndirect, DeviceSortBufferType::kPayloadKey32, DeviceSortBufferType::kPayload> config;
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto [sorter, sortResource] = makeSorter(config, ctx.getDevice(), count);
    auto [cpuBuffer, pGpuBuffers] = makeBuffers(config, ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    for (uint i = 0, sortCount = 0; i < 5; ++i)
    {
        sortCount = std::uniform_int_distribution<uint>{sortCount, count}(rand);
        fmt::println("Sort Count = {}", sortCount);
        runTest(config, ctx, sorter, sortResource, cpuBuffer, pGpuBuffers, sortCount);
    }
}

GPU_TEST(DeviceSorter_Mix_Direct)
{
    Config<
        DeviceSortDispatchType::kDirect,
        DeviceSortBufferType::kPayloadKey32,
        DeviceSortBufferType::kPayloadKey32,
        DeviceSortBufferType::kPayload>
        config;
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto [sorter, sortResource] = makeSorter(config, ctx.getDevice(), count);
    auto [cpuBuffer, pGpuBuffers] = makeBuffers(config, ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    runTest(config, ctx, sorter, sortResource, cpuBuffer, pGpuBuffers, count);
}

} // namespace GSGI
