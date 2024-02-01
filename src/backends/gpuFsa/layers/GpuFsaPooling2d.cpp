//
// Copyright © 2024 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "GpuFsaPooling2d.hpp"
#include "UtilsGpuFsa.hpp"

#include <aclCommon/ArmComputeTensorUtils.hpp>

#include <arm_compute/dynamic_fusion/sketch/gpu/GpuWorkloadContext.h>
#include <arm_compute/dynamic_fusion/sketch/gpu/GpuWorkloadSketch.h>
#include <arm_compute/dynamic_fusion/sketch/gpu/operators/GpuPool2d.h>
#include <arm_compute/dynamic_fusion/sketch/gpu/operators/GpuOutput.h>

using namespace arm_compute::experimental::dynamic_fusion;
using namespace armnn::armcomputetensorutils;

namespace armnn
{

arm_compute::Status GpuFsaPooling2dValidate(const TensorInfo& input,
                                            const Pooling2dDescriptor& descriptor)
{
    // Create a new workload sketch, for validation purposes
    auto compileCtx         = arm_compute::CLKernelLibrary::get().get_compile_context();
    auto workloadContext    = GpuWorkloadContext(&compileCtx);
    GpuWorkloadSketch sketch{ &workloadContext };

    arm_compute::TensorInfo aclInputInfo = BuildArmComputeTensorInfo(input, descriptor.m_DataLayout);
    aclInputInfo.set_are_values_constant(input.IsConstant());
    arm_compute::ITensorInfo* inputInfo = workloadContext.create_tensor_info(aclInputInfo);

    Pool2dAttributes pool2dAttributes = CreatePool2dAttributes(descriptor);
    GpuPool2dSettings pool2dSettings{};

    return GpuPool2d::validate_op(sketch, inputInfo, pool2dAttributes, pool2dSettings);
}

void GpuFsaPooling2dCreateOp(GpuFsaPreCompiledBlob* blob,
                             const TensorInfo& input,
                             const Pooling2dDescriptor& descriptor)
{
    GpuWorkloadSketch* sketch           = blob->sketch.get();
    GpuWorkloadContext* workloadContext = blob->workloadContext.get();
    std::vector<arm_compute::ITensorInfo*> inputTensorInfos  = {};
    std::vector<arm_compute::ITensorInfo*> outputTensorInfos = {};

    arm_compute::TensorInfo aclInputInfo = BuildArmComputeTensorInfo(input, descriptor.m_DataLayout);
    aclInputInfo.set_are_values_constant(input.IsConstant());

    inputTensorInfos.emplace_back(workloadContext->create_tensor_info(aclInputInfo));

    Pool2dAttributes pool2dAttributes = CreatePool2dAttributes(descriptor);
    GpuPool2dSettings pool2dSettings{};

    // Validate operator, check status and update reasonIfUnsupported
    arm_compute::Status aclStatus = GpuPool2d::validate_op(*sketch,
                                                           inputTensorInfos[0],
                                                           pool2dAttributes,
                                                           pool2dSettings);

    const bool supported = aclStatus.error_code() == arm_compute::ErrorCode::OK;
    if (!supported)
    {
        throw BackendCapabilityException("\"GpuFsa\" backend failed during pooling 2d validation");
    }

    arm_compute::ITensorInfo* addOutputInfo = GpuPool2d::create_op(*sketch,
                                                                   inputTensorInfos[0],
                                                                   pool2dAttributes,
                                                                   pool2dSettings);

    // Temporary fix until fusing attempt is make for GpuFsa backend and Output layer workload is created.
    outputTensorInfos.emplace_back(workloadContext->create_tensor_info());
    GpuOutput::create_op(*sketch, addOutputInfo, outputTensorInfos[0]);

    // Store the TensorInfos within the blob as unique_ptrs to be used later
    blob->inputTensorInfos  = std::make_unique<std::vector<arm_compute::ITensorInfo*>>(inputTensorInfos);
    blob->outputTensorInfos = std::make_unique<std::vector<arm_compute::ITensorInfo*>>(outputTensorInfos);
}

} // namespace armnn