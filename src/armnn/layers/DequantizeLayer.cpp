//
// Copyright © 2017,2019-2023 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include "DequantizeLayer.hpp"

#include "LayerCloneBase.hpp"

#include <armnn/backends/WorkloadData.hpp>
#include <armnn/backends/WorkloadFactory.hpp>

namespace armnn
{

DequantizeLayer::DequantizeLayer(const char* name)
    : Layer(1, 1, LayerType::Dequantize, name)
{}

std::unique_ptr<IWorkload> DequantizeLayer::CreateWorkload(
                                                           const IWorkloadFactory& factory) const
{
    DequantizeQueueDescriptor descriptor;
    SetAdditionalInfo(descriptor);

    return factory.CreateWorkload(LayerType::Dequantize, descriptor, PrepInfoAndDesc(descriptor));
}

DequantizeLayer* DequantizeLayer::Clone(Graph& graph) const
{
    return CloneBase<DequantizeLayer>(graph, GetName());
}

void DequantizeLayer::ValidateTensorShapesFromInputs()
{
    VerifyLayerConnections(1, CHECK_LOCATION());

    const TensorShape& outputShape = GetOutputSlot(0).GetTensorInfo().GetShape();

    VerifyShapeInferenceType(outputShape, m_ShapeInferenceMethod);

    std::vector<TensorShape> inferredShapes = InferOutputShapes({
        GetInputSlot(0).GetTensorInfo().GetShape() });

    ARMNN_ASSERT(inferredShapes.size() == 1);

    ValidateAndCopyShape(outputShape, inferredShapes[0], m_ShapeInferenceMethod, "DequantizeLayer");
}

void DequantizeLayer::ExecuteStrategy(IStrategy& strategy) const
{
    strategy.ExecuteStrategy(this, GetParameters(), {}, GetName());
}

} // namespace armnn
