//
// Created by adamyuan on 12/20/24.
//

#include "Falcor.h"
#include "Testing/UnitTest.h"

using namespace Falcor;

namespace GSGI
{

GPU_TEST(IndirectDispatch_Test)
{
    DispatchArguments args = {1, 1, 1};
    auto pArgBuffer = ctx.getDevice()->createStructuredBuffer(
        sizeof(DispatchArguments), 1, ResourceBindFlags::IndirectArg, MemoryType::DeviceLocal, &args
    );

    auto pPass = ComputePass::create(ctx.getDevice(), "GaussianGI/Test/IndirectDispatch.cs.slang", "csMain");
    pPass->executeIndirect(ctx.getRenderContext(), pArgBuffer.get(), 0);
}

} // namespace GSGI
