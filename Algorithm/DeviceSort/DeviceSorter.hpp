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

class DeviceSortDesc
{
public:
    struct PassDesc
    {
        uint32_t keyBufferID;
        uint32_t keyRadixShift;
        bool keyWrite;
        std::vector<uint32_t> payloadBufferIDs;
    };

private:
    uint32_t mBufferCount;
    std::vector<PassDesc> mPassDescs;

public:
    explicit DeviceSortDesc(const std::vector<DeviceSortBufferType>& bufferTypes);
    uint32_t getBufferCount() const { return mBufferCount; }
    uint32_t getPassCount() const { return mPassDescs.size(); }
    const auto& getPassDescs() const { return mPassDescs; }
};

enum class DeviceSortType
{
    kKey,
    kPair
};

enum class DeviceSortDispatchType
{
    kDirect,
    kIndirect
};

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
struct DeviceSorterResource
{
    uint maxCount{};
    ref<Buffer> pTempKeyBuffer;
    ref<Buffer> pTempPayloadBuffer;
    ref<Buffer> pGlobalHistBuffer;
    ref<Buffer> pPassHistBuffer;
    ref<Buffer> pIndexBuffer;
    ref<Buffer> pIndirectBuffer;

    static DeviceSorterResource create(const ref<Device>& pDevice, uint maxCount);
    template<DeviceSortType AnotherType_V, DeviceSortDispatchType AnotherDispatchType_V>
    DeviceSorterResource<AnotherType_V, AnotherDispatchType_V> convertTo()
        requires(
            static_cast<uint>(AnotherType_V) <= static_cast<uint>(Type_V) &&
            static_cast<uint>(AnotherDispatchType_V) <= static_cast<uint>(DispatchType_V)
            // Ensure no extra buffer is required
        )
    {
        return DeviceSorterResource<AnotherType_V, AnotherDispatchType_V>{
            .maxCount = maxCount,
            .pTempKeyBuffer = pTempKeyBuffer,
            .pTempPayloadBuffer = pTempPayloadBuffer,
            .pGlobalHistBuffer = pGlobalHistBuffer,
            .pPassHistBuffer = pPassHistBuffer,
            .pIndexBuffer = pIndexBuffer,
            .pIndirectBuffer = pIndirectBuffer,
        };
    }
    bool isCapable(uint maxCount) const;
};

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
class DeviceSorter final
{
private:
    ref<ComputePass> mpResetPass, mpGlobalHistPass, mpScanHistPass, mpOneSweepPass;

    static void bindCount(const ShaderVar& var, uint count, const ref<Buffer>& pCountBuffer, uint64_t countBufferOffset);

    void dispatchImpl(
        ComputeContext* pComputeContext,
        const ref<Buffer>& pKeyBuffer,
        const ref<Buffer>& pPayloadBuffer,
        uint count,
        const ref<Buffer>& pCountBuffer,
        uint64_t countBufferOffset,
        const DeviceSorterResource<Type_V, DispatchType_V>& resource
    );

public:
    explicit DeviceSorter(const ref<Device>& pDevice);

    void dispatch(
        ComputeContext* pComputeContext, //
        const ref<Buffer>& pKeyBuffer,
        uint count,
        const DeviceSorterResource<Type_V, DispatchType_V>& resource
    )
        requires(Type_V == DeviceSortType::kKey && DispatchType_V == DeviceSortDispatchType::kDirect)
    {
        dispatchImpl(pComputeContext, pKeyBuffer, nullptr, count, nullptr, 0, resource);
    }

    void dispatch(
        ComputeContext* pComputeContext,
        const ref<Buffer>& pKeyBuffer,
        const ref<Buffer>& pPayloadBuffer,
        uint count,
        const DeviceSorterResource<Type_V, DispatchType_V>& resource
    )
        requires(Type_V == DeviceSortType::kPair && DispatchType_V == DeviceSortDispatchType::kDirect)
    {
        dispatchImpl(pComputeContext, pKeyBuffer, pPayloadBuffer, count, nullptr, 0, resource);
    }

    void dispatch(
        ComputeContext* pComputeContext,
        const ref<Buffer>& pKeyBuffer,
        const ref<Buffer>& pCountBuffer,
        uint64_t countBufferOffset,
        const DeviceSorterResource<Type_V, DispatchType_V>& resource
    )
        requires(Type_V == DeviceSortType::kKey && DispatchType_V == DeviceSortDispatchType::kIndirect)
    {
        dispatchImpl(pComputeContext, pKeyBuffer, nullptr, 0, pCountBuffer, countBufferOffset, resource);
    }

    void dispatch(
        ComputeContext* pComputeContext,
        const ref<Buffer>& pKeyBuffer,
        const ref<Buffer>& pPayloadBuffer,
        const ref<Buffer>& pCountBuffer,
        uint64_t countBufferOffset,
        const DeviceSorterResource<Type_V, DispatchType_V>& resource
    )
        requires(Type_V == DeviceSortType::kPair && DispatchType_V == DeviceSortDispatchType::kIndirect)
    {
        dispatchImpl(pComputeContext, pKeyBuffer, pPayloadBuffer, 0, pCountBuffer, countBufferOffset, resource);
    }
};

} // namespace GSGI

#endif // GSGI_ONESWEEPSORTER_HPP
