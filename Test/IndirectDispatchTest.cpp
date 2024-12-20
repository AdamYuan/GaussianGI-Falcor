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

    ProgramDesc::ShaderModule shaderModule;
    shaderModule.addString(R"(
[numthreads(1, 1, 1)]
void main() {}
)");

    ProgramDesc desc = {};
    desc.addShaderModule(shaderModule).csEntry("main");

    auto pPass = ComputePass::create(ctx.getDevice(), desc);
    pPass->execute(ctx.getRenderContext(), 1, 1, 1); // OK
    fmt::println("ComputePass::execute");
    pPass->executeIndirect(ctx.getRenderContext(), pArgBuffer.get(), 0); // Crash
    fmt::println("ComputePass::executeIndirect");
}

} // namespace GSGI
