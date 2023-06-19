//
// Copyright © 2017, 2019-2023 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ClBatchToSpaceNdWorkload.hpp"

#include <armnn/utility/PolymorphicDowncast.hpp>

#include <cl/ClTensorHandle.hpp>

namespace armnn
{

using namespace armcomputetensorutils;

arm_compute::Status ClBatchToSpaceNdWorkloadValidate(const TensorInfo& input,
                                                     const TensorInfo& output,
                                                     const BatchToSpaceNdDescriptor& descriptor)
{
    arm_compute::TensorInfo aclInputInfo = BuildArmComputeTensorInfo(input, descriptor.m_DataLayout);
    arm_compute::TensorInfo aclOutputInfo = BuildArmComputeTensorInfo(output, descriptor.m_DataLayout);

    arm_compute::Status statusBatchToSpace  = arm_compute::Status(arm_compute::ErrorCode::OK);
    arm_compute::Status statusReshapeInput  = arm_compute::Status(arm_compute::ErrorCode::OK);
    arm_compute::Status statusReshapeOutput = arm_compute::Status(arm_compute::ErrorCode::OK);

    arm_compute::TensorInfo aclReshapeInputInfo  = aclInputInfo;
    arm_compute::TensorInfo aclReshapeOutputInfo = aclOutputInfo;

    // When a spacial dimension is missing (rank=3) set W to 1
    const unsigned int rank = input.GetNumDimensions();
    if (rank == 3)
    {
        const arm_compute::TensorShape inputShape = aclInputInfo.tensor_shape();
        const arm_compute::TensorShape outputShape = aclOutputInfo.tensor_shape();

        if (descriptor.m_DataLayout == armnn::DataLayout::NHWC)
        {
            // In ACL dimensions are right to left: C, W, H, N
            aclInputInfo.set_tensor_shape({inputShape.x(), 1, inputShape.y(), inputShape.z()});
            aclOutputInfo.set_tensor_shape({outputShape.x(), 1, outputShape.y(), outputShape.z()});
        }
        else if (descriptor.m_DataLayout == armnn::DataLayout::NCHW)
        {
            // In ACL dimensions are right to left: W, H, C, N
            aclInputInfo.set_tensor_shape({1, inputShape.x(), inputShape.y(), inputShape.z()});
            aclOutputInfo.set_tensor_shape({1, outputShape.x(), outputShape.y(), outputShape.z()});
        }
        else
        {
            throw InvalidArgumentException("Unsupported or unknown DataLayout", CHECK_LOCATION());
        }

        statusReshapeInput = arm_compute::CLReshapeLayer::validate(&aclInputInfo, &aclReshapeInputInfo);
        statusReshapeOutput = arm_compute::CLReshapeLayer::validate(&aclReshapeOutputInfo, &aclOutputInfo);
    }

    // ArmNN blockShape is [H, W] ACl asks for W, H
    int32_t blockHeight = armnn::numeric_cast<int32_t>(descriptor.m_BlockShape[0]);
    int32_t blockWidth = (rank == 3) ? 1 : armnn::numeric_cast<int32_t>(descriptor.m_BlockShape[1]);

    const arm_compute::CropInfo cropInfo = BuildArmComputeCropInfo(descriptor, rank);

    statusBatchToSpace = arm_compute::CLBatchToSpaceLayer::validate(rank == 3 ? &aclReshapeInputInfo : &aclInputInfo,
                                                                    blockWidth,
                                                                    blockHeight,
                                                                    rank == 3 ? &aclReshapeOutputInfo : &aclOutputInfo,
                                                                    cropInfo);

    if (statusReshapeInput.error_code()  == arm_compute::ErrorCode::OK &&
        statusReshapeOutput.error_code() == arm_compute::ErrorCode::OK &&
        statusBatchToSpace.error_code()  == arm_compute::ErrorCode::OK)
    {
        return arm_compute::Status(arm_compute::ErrorCode::OK,
                                   "All BatchToSpace layers validate status OK.");
    }
    else
    {
        return arm_compute::Status(arm_compute::ErrorCode::RUNTIME_ERROR,
                                   "BatchToSpace layer validate status failed."
                                   + statusBatchToSpace.error_description()
                                   + statusReshapeInput.error_description()
                                   + statusReshapeOutput.error_description());
    }
}

ClBatchToSpaceNdWorkload::ClBatchToSpaceNdWorkload(const BatchToSpaceNdQueueDescriptor& descriptor,
                                                   const WorkloadInfo& info,
                                                   const arm_compute::CLCompileContext& clCompileContext)
    : ClBaseWorkload<BatchToSpaceNdQueueDescriptor>(descriptor, info)
{
    // Report Profiling Details
    ARMNN_REPORT_PROFILING_WORKLOAD_DESC("ClBatchToSpaceWorkload_Construct",
                                         descriptor.m_Parameters,
                                         info,
                                         this->GetGuid());

    m_Data.ValidateInputsOutputs("ClBatchToSpaceNdWorkload", 1, 1);

    arm_compute::ICLTensor& input = static_cast<IClTensorHandle*>(m_Data.m_Inputs[0])->GetTensor();
    arm_compute::ICLTensor& output = static_cast<IClTensorHandle*>(m_Data.m_Outputs[0])->GetTensor();

    arm_compute::DataLayout aclDataLayout = ConvertDataLayout(m_Data.m_Parameters.m_DataLayout);
    input.info()->set_data_layout(aclDataLayout);
    output.info()->set_data_layout(aclDataLayout);

    arm_compute::TensorInfo aclReshapeInputInfo = BuildArmComputeTensorInfo(info.m_InputTensorInfos[0],
                                                                            m_Data.m_Parameters.m_DataLayout);
    arm_compute::TensorInfo aclReshapeOutputInfo = BuildArmComputeTensorInfo(info.m_OutputTensorInfos[0],
                                                                             m_Data.m_Parameters.m_DataLayout);

    const unsigned int rank = info.m_InputTensorInfos[0].GetNumDimensions();
    if (rank == 3)
    {
        const arm_compute::TensorShape inputShape  = input.info()->tensor_shape();
        const arm_compute::TensorShape outputShape = output.info()->tensor_shape();

        // When a spacial dimension is missing set W to 1
        if (m_Data.m_Parameters.m_DataLayout == armnn::DataLayout::NHWC)
        {
            // In ACL dimensions are right to left: C, W, H, N
            aclReshapeInputInfo.set_tensor_shape({inputShape.x(), 1, inputShape.y(), inputShape.z()});
            aclReshapeOutputInfo.set_tensor_shape({outputShape.x(), 1, outputShape.y(), outputShape.z()});
        }
        else if (m_Data.m_Parameters.m_DataLayout == armnn::DataLayout::NCHW)
        {
            // In ACL dimensions are right to left: W, H, C, N
            aclReshapeInputInfo.set_tensor_shape({1, inputShape.x(), inputShape.y(), inputShape.z()});
            aclReshapeOutputInfo.set_tensor_shape({1, outputShape.x(), outputShape.y(), outputShape.z()});
        }
        else
        {
            throw InvalidArgumentException("Unsupported or unknown DataLayout", CHECK_LOCATION());
        }

        m_ReshapeInputTensor.allocator()->init(aclReshapeInputInfo);
        m_ReshapeOutputTensor.allocator()->init(aclReshapeOutputInfo);

        InitialiseArmComputeTensorEmpty(m_ReshapeInputTensor);
        InitialiseArmComputeTensorEmpty(m_ReshapeOutputTensor);

        m_LayerReshapeInput.reset(new arm_compute::CLReshapeLayer());
        m_LayerReshapeOutput.reset(new arm_compute::CLReshapeLayer());

        m_LayerReshapeInput->configure(clCompileContext, &input, &m_ReshapeInputTensor);
        m_LayerReshapeOutput->configure(clCompileContext, &m_ReshapeOutputTensor, &output);
    }

    // ArmNN blockShape is [H, W] ACl asks for W, H
    int32_t blockHeight = armnn::numeric_cast<int32_t>(descriptor.m_Parameters.m_BlockShape[0]);
    int32_t blockWidth = (rank == 3) ? 1 : armnn::numeric_cast<int32_t>(descriptor.m_Parameters.m_BlockShape[1]);

    const arm_compute::CropInfo cropInfo = BuildArmComputeCropInfo(descriptor.m_Parameters);

    {
        ARMNN_SCOPED_PROFILING_EVENT(Compute::Undefined, "ClBatchToSpaceNdWorkload_configure");
        m_Layer.configure(clCompileContext,
                          (rank == 3) ? &m_ReshapeInputTensor : &input,
                          blockWidth,
                          blockHeight,
                          (rank == 3) ? &m_ReshapeOutputTensor : &output,
                          cropInfo);
    }
}

void ClBatchToSpaceNdWorkload::Execute() const
{
    ARMNN_SCOPED_PROFILING_EVENT_CL_GUID("ClBatchToSpaceNdWorkload_Execute", this->GetGuid());
    if (m_LayerReshapeInput)
    {
        m_LayerReshapeInput->run();
    }
    RunClFunction(m_Layer, CHECK_LOCATION());
    if (m_LayerReshapeOutput)
    {
        m_LayerReshapeOutput->run();
    }
}

} //namespace armnn
