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
std::mt19937 rand{std::random_device{}()};

std::tuple<std::vector<uint32_t>, ref<Buffer>> makeKeys(const ref<Device>& pDevice, uint count)
{
    std::vector<uint32_t> keys(count);
    for (auto& key : keys)
        key = std::uniform_int_distribution<uint32_t>{}(rand);

    auto pKeyBuffer = pDevice->createStructuredBuffer(
        sizeof(uint32_t),
        count,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal,
        keys.data()
    );
    return {std::move(keys), std::move(pKeyBuffer)};
}

std::tuple<std::vector<uint32_t>, ref<Buffer>> makePayloads(const ref<Device>& pDevice, uint count)
{
    std::vector<uint32_t> payloads(count);
    for (uint i = 0; i < count; ++i)
        payloads[i] = i;
    auto pPayloadBuffer = pDevice->createStructuredBuffer(
        sizeof(uint32_t),
        count,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal,
        payloads.data()
    );
    return {std::move(payloads), std::move(pPayloadBuffer)};
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

void validate(GPUUnitTestContext& ctx, const std::vector<uint32_t>& keys, const ref<Buffer>& pKeyBuffer, uint sortCount)
{
    auto sortedKeys = std::vector<uint32_t>{keys.begin(), keys.begin() + sortCount};
    std::sort(sortedKeys.begin(), sortedKeys.end());
    auto gpuSortedKeys = pKeyBuffer->getElements<uint32_t>(0, sortCount);
    for (uint i = 0; i < sortCount; ++i)
        EXPECT_EQ(sortedKeys[i], gpuSortedKeys[i]);
}

void validate(
    GPUUnitTestContext& ctx,
    const std::vector<uint32_t>& keys,
    const std::vector<uint32_t>& payloads,
    const ref<Buffer>& pKeyBuffer,
    const ref<Buffer>& pPayloadBuffer,
    uint sortCount
)
{
    std::vector<std::pair<uint, uint>> sortedPairs(sortCount);
    for (uint i = 0; i < sortCount; ++i)
        sortedPairs[i] = {keys[i], payloads[i]};
    std::stable_sort(sortedPairs.begin(), sortedPairs.end(), [](const auto& l, const auto& r) { return l.first < r.first; });
    auto gpuSortedKeys = pKeyBuffer->getElements<uint32_t>(0, sortCount);
    auto gpuSortedPayloads = pPayloadBuffer->getElements<uint32_t>(0, sortCount);
    for (uint i = 0; i < sortCount; ++i)
    {
        EXPECT_EQ(sortedPairs[i].first, gpuSortedKeys[i]);
        EXPECT_EQ(sortedPairs[i].second, gpuSortedPayloads[i]);
    }
}

} // namespace

GPU_TEST(DeviceSorter_Key_Direct)
{
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto sorter = DeviceSorter<DeviceSortType::kKey, DeviceSortDispatchType::kDirect>(ctx.getDevice());
    auto sorterResource = DeviceSorterResource<DeviceSortType::kKey, DeviceSortDispatchType::kDirect>::create(ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    auto [keys, pKeyBuffer] = makeKeys(ctx.getDevice(), count);
    sorter.dispatch(ctx.getRenderContext(), pKeyBuffer, count, sorterResource);
    validate(ctx, keys, pKeyBuffer, count);
}

GPU_TEST(DeviceSorter_Pair_Direct)
{
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto sorter = DeviceSorter<DeviceSortType::kPair, DeviceSortDispatchType::kDirect>(ctx.getDevice());
    auto sorterResource = DeviceSorterResource<DeviceSortType::kPair, DeviceSortDispatchType::kDirect>::create(ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    auto [keys, pKeyBuffer] = makeKeys(ctx.getDevice(), count);
    auto [payloads, pPayloadBuffer] = makePayloads(ctx.getDevice(), count);
    sorter.dispatch(ctx.getRenderContext(), pKeyBuffer, pPayloadBuffer, count, sorterResource);
    validate(ctx, keys, payloads, pKeyBuffer, pPayloadBuffer, count);
}

GPU_TEST(DeviceSorter_Pair_IsStableSort)
{
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto sorter = DeviceSorter<DeviceSortType::kPair, DeviceSortDispatchType::kDirect>(ctx.getDevice());
    auto sorterResource = DeviceSorterResource<DeviceSortType::kPair, DeviceSortDispatchType::kDirect>::create(ctx.getDevice(), count);
    fmt::println("Count = {}", count);
    std::vector<uint32_t> keys(count);
    for (auto& key : keys)
        key = std::uniform_int_distribution<uint32_t>{1, 4}(rand);
    auto pKeyBuffer = ctx.getDevice()->createStructuredBuffer(
        sizeof(uint32_t),
        count,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal,
        keys.data()
    );
    auto [payloads, pPayloadBuffer] = makePayloads(ctx.getDevice(), count);
    sorter.dispatch(ctx.getRenderContext(), pKeyBuffer, pPayloadBuffer, count, sorterResource);
    validate(ctx, keys, payloads, pKeyBuffer, pPayloadBuffer, count);
}

GPU_TEST(DeviceSorter_Key_Indirect)
{
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto sorter = DeviceSorter<DeviceSortType::kKey, DeviceSortDispatchType::kIndirect>(ctx.getDevice());
    auto sorterResource = DeviceSorterResource<DeviceSortType::kKey, DeviceSortDispatchType::kIndirect>::create(ctx.getDevice(), count);
    fmt::println("Count = {}", count);

    auto [keys, pKeyBuffer] = makeKeys(ctx.getDevice(), count);

    for (uint i = 0, sortCount = 0; i < 5; ++i)
    {
        sortCount = std::uniform_int_distribution<uint>{sortCount, count}(rand);
        fmt::println("Sort Count = {}", sortCount);
        auto [pCountBuffer, countBufferOffset] = makeCountBuffer(ctx.getDevice(), sortCount);
        sorter.dispatch(ctx.getRenderContext(), pKeyBuffer, pCountBuffer, countBufferOffset, sorterResource);
        validate(ctx, keys, pKeyBuffer, sortCount);
    }
}

GPU_TEST(DeviceSorter_Pair_Indirect)
{
    uint count = std::uniform_int_distribution<uint>{1024 * 1024, 8 * 1024 * 1024}(rand);
    auto sorter = DeviceSorter<DeviceSortType::kPair, DeviceSortDispatchType::kIndirect>(ctx.getDevice());
    auto sorterResource = DeviceSorterResource<DeviceSortType::kPair, DeviceSortDispatchType::kIndirect>::create(ctx.getDevice(), count);
    fmt::println("Count = {}", count);

    auto [keys, pKeyBuffer] = makeKeys(ctx.getDevice(), count);
    auto [payloads, pPayloadBuffer] = makePayloads(ctx.getDevice(), count);

    for (uint i = 0, sortCount = 0; i < 5; ++i)
    {
        sortCount = std::uniform_int_distribution<uint>{sortCount, count}(rand);
        fmt::println("Sort Count = {}", sortCount);
        auto [pCountBuffer, countBufferOffset] = makeCountBuffer(ctx.getDevice(), sortCount);
        sorter.dispatch(ctx.getRenderContext(), pKeyBuffer, pPayloadBuffer, pCountBuffer, countBufferOffset, sorterResource);
        validate(ctx, keys, payloads, pKeyBuffer, pPayloadBuffer, sortCount);
    }
}

} // namespace GSGI
