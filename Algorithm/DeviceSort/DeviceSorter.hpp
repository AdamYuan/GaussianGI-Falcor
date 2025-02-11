//
// Created by adamyuan on 12/19/24.
//

#ifndef GSGI_ONESWEEPSORTER_HPP
#define GSGI_ONESWEEPSORTER_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

enum class DeviceSortBufferType : uint8_t
{
    kPayload = 1,
    kKey16 = 16, // use first 16-bit in each 32-bit integer
    kPayloadKey16 = 16 | 1,
    kKey32 = 32, // use full 32-bit integer
    kPayloadKey32 = 32 | 1,
    // Key exclusively don't guarantee ordering after sort
};

struct DeviceSortBufferTypeMethod
{
    static constexpr uint32_t getKeyBitWidth(DeviceSortBufferType type) { return static_cast<uint32_t>(type) & (~1u); }
    static constexpr bool isKey(DeviceSortBufferType type) { return getKeyBitWidth(type); }
    static constexpr bool isPayload(DeviceSortBufferType type) { return static_cast<uint32_t>(type) & 1u; }
};

class DeviceSortDesc
{
public:
    struct KeyBufferDesc
    {
        uint32_t bufferID; // Index in buffer array
        uint32_t bitWidth;
        bool isPayload;
        std::vector<uint32_t> payloadBufferIDs;
        uint32_t firstPassID; // First index in mPassDescs
        uint32_t passCount;
    };

    struct PassDesc
    {
        uint32_t keyID; // Index in mKeyBufferDescs
        uint32_t keyRadixShift;
        bool keyWrite;
    };

private:
    uint32_t mBufferCount{};
    std::vector<PassDesc> mPassDescs;
    std::vector<KeyBufferDesc> mKeyBufferDescs;

public:
    DeviceSortDesc() = default;
    explicit DeviceSortDesc(const std::vector<DeviceSortBufferType>& bufferTypes);
    uint32_t getBufferCount() const { return mBufferCount; }
    uint32_t getPassCount() const { return mPassDescs.size(); }
    const auto& getPassDesc(uint32_t passID) const { return mPassDescs[passID]; }
    uint32_t getKeyBufferCount() const { return mKeyBufferDescs.size(); }
    const auto& getKeyBufferDesc(uint32_t keyID) const { return mKeyBufferDescs[keyID]; }
    uint32_t getPassMaxPayloadBufferCount() const { return getBufferCount() - 1; }
};

enum class DeviceSortDispatchType
{
    kDirect,
    kIndirect
};

template<DeviceSortDispatchType DispatchType_V>
struct DeviceSortResource
{
    uint maxPassCount{}, maxKeyCount{};
    std::vector<ref<Buffer>> pTempBuffers;
    ref<Buffer> pGlobalHistBuffer;
    ref<Buffer> pPassHistBuffer;
    ref<Buffer> pIndexBuffer;
    ref<Buffer> pIndirectBuffer;

    static DeviceSortResource create(const ref<Device>& pDevice, const DeviceSortDesc& desc, uint maxKeyCount)
    {
        return create(pDevice, desc.getBufferCount(), desc.getPassCount(), maxKeyCount);
    }
    static DeviceSortResource create(const ref<Device>& pDevice, uint maxBufferCount, uint maxPassCount, uint maxKeyCount);

    template<DeviceSortDispatchType AnotherDispatchType_V>
    DeviceSortResource<AnotherDispatchType_V> convertTo()
        requires(
            static_cast<uint>(AnotherDispatchType_V) <= static_cast<uint>(DispatchType_V)
            // Ensure no extra buffer is required
        )
    {
        return DeviceSortResource<AnotherDispatchType_V>{
            .maxPassCount = maxPassCount,
            .maxKeyCount = maxKeyCount,
            .pTempBuffers = pTempBuffers,
            .pGlobalHistBuffer = pGlobalHistBuffer,
            .pPassHistBuffer = pPassHistBuffer,
            .pIndexBuffer = pIndexBuffer,
            .pIndirectBuffer = pIndirectBuffer,
        };
    }
    bool isCapable(const DeviceSortDesc& desc, uint keyCount) const
    {
        return isCapable(desc.getBufferCount(), desc.getPassCount(), keyCount);
    }
    bool isCapable(uint bufferCount, uint passCount, uint keyCount) const;
};

template<DeviceSortDispatchType DispatchType_V>
class DeviceSorter final
{
private:
    DeviceSortDesc mDesc;
    ref<ComputePass> mpResetPass, mpGlobalHistPass, mpScanHistPass, mpOneSweepPass;

    static void bindCount(const ShaderVar& var, uint count, const ref<Buffer>& pCountBuffer, uint64_t countBufferOffset);

    void dispatchImpl(
        ComputeContext* pComputeContext,
        const std::vector<ref<Buffer>>& pBuffers,
        uint count,
        const ref<Buffer>& pCountBuffer,
        uint64_t countBufferOffset,
        const DeviceSortResource<DispatchType_V>& resource
    ) const;

public:
    DeviceSorter() = default;
    DeviceSorter(const ref<Device>& pDevice, DeviceSortDesc desc);

    bool isInitialized() const;

    const auto& getDesc() const { return mDesc; }

    void dispatch(
        ComputeContext* pComputeContext,
        const std::vector<ref<Buffer>>& pBuffers,
        uint count,
        const DeviceSortResource<DispatchType_V>& resource
    ) const
        requires(DispatchType_V == DeviceSortDispatchType::kDirect)
    {
        dispatchImpl(pComputeContext, pBuffers, count, nullptr, 0, resource);
    }

    void dispatch(
        ComputeContext* pComputeContext,
        const std::vector<ref<Buffer>>& pBuffers,
        const ref<Buffer>& pCountBuffer,
        uint64_t countBufferOffset,
        const DeviceSortResource<DispatchType_V>& resource
    ) const
        requires(DispatchType_V == DeviceSortDispatchType::kIndirect)
    {
        dispatchImpl(pComputeContext, pBuffers, 0, pCountBuffer, countBufferOffset, resource);
    }
};

} // namespace GSGI

#endif // GSGI_ONESWEEPSORTER_HPP
