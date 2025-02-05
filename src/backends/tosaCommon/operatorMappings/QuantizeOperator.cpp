//
// Copyright © 2023 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Copyright © 2020 The TensorFlow Authors. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
//

#include "QuantizeOperator.hpp"

// This function is paraphrased from:
// tensorflow/compiler/mlir/tosa/transforms/legalize_common.cc from function convertQuantizeOp
TosaSerializationBasicBlock* ConvertQuantizeToTosaOperator(const Layer* layer,
                                                           const std::vector<const TensorInfo*>& inputs,
                                                           const std::vector<const TensorInfo*>& outputs)
{
    ARMNN_THROW_INVALIDARG_MSG_IF_FALSE( inputs.size() == 1,
                                         "ConvertQuantizeToTosaOperator: Quantize must have only one input" );
    ARMNN_THROW_INVALIDARG_MSG_IF_FALSE( outputs.size() == 1,
                                         "ConvertQuantizeToTosaOperator: Quantize must have only one output" );

    std::string inputName           = std::string("input0_");
    std::string outputNameZeroPoint = std::string("intermediate0_") + GetUniqueTosaMappingID();
    std::string outputNameScale     = std::string("intermediate1_") + GetUniqueTosaMappingID();
    std::string outputNameMul       = std::string("intermediate2_") + GetUniqueTosaMappingID();
    std::string outputNameAdd       = std::string("intermediate3_") + GetUniqueTosaMappingID();
    std::string outputName          = std::string("output0_");
    std::string blockName           = std::string("Op_QUANTIZE_block_") + GetUniqueTosaMappingID();

    // If a layer is present then the block will be used for execution, so input and output names need to be determined
    // using the previous and following layers so the graph is connected correctly. For validation this doesn't matter.
    if(layer != nullptr)
    {
        // Get the layers connected to the input slots and determine unique tensor names.
        Layer& connectedLayer = layer->GetInputSlot(0).GetConnectedOutputSlot()->GetOwningLayer();
        inputName = GenerateUniqueName(connectedLayer, 0);

        // Determine unique output tensor name.
        outputName = GenerateUniqueOutputName(*layer, 0);
    }

    const TensorInfo inputInfo = *inputs[0];
    const TensorInfo outputInfo = *outputs[0];

    // Extract quantization detail from Tensor
    float zeroPoint = static_cast<float>(outputInfo.GetQuantizationOffset());
    // No per axis support in Tensorflow TOSA code
    float scale = outputInfo.GetQuantizationScale();

    // As per the Tensorflow quantization specification
    // Tensorflow TOSA code calculates quantization using multiplication by scale
    // Armnn code calculates quantization using division by scale
    // Invert scale factor passed from Armnn for tf TOSA code
    scale = (scale != 0) ?  (1 / scale) : scale;

    std::vector<TosaSerializationTensor*> tensors;

    // Only add input tensors if connected layer is an input layer.
    // As intermediate or constant tensors will be created separately.
    // There also can't be duplicate tensor.
    std::vector<int32_t> inputShape0;
    DType inputDType0 =  DType::DType_UNKNOWN;
    if(inputName.find("input0_") != std::string::npos)
    {
        inputShape0 = GetTosaTensorShape(inputInfo.GetShape());
        inputDType0 = ArmNNToDType(inputInfo.GetDataType());
        ARMNN_THROW_INVALIDARG_MSG_IF_FALSE( inputDType0 == DType::DType_FP16 || inputDType0 == DType::DType_FP32,
                                             "ConvertQuantizeToTosaOperator: Quantize input must be of type Float" );
        tensors.push_back(new TosaSerializationTensor(inputName, inputShape0, inputDType0, {}));
    }

    std::vector<int32_t> outputShape0 = GetTosaTensorShape(outputInfo.GetShape());
    DType outputDType0 = ArmNNToDType(outputInfo.GetDataType());

    // quantize:
    // const_zeroPoint = constant(zeroPoint)
    // const_scale = constant(scale)
    // out_mul = mul(input, const_scale)
    // out_add = add(out_mul, const_zeroPoint)
    // output = cast<output_type>(out_add)

    // const_zeroPoint
    TosaSerializationOperator* zeroPointOp = nullptr;
    TosaSerializationTensor* zeroPointTensor = nullptr;
    CreateConstTosaOperator<float>(outputNameZeroPoint,
                                   zeroPoint,
                                   inputDType0,
                                   inputShape0,
                                   zeroPointOp,
                                   zeroPointTensor);
    tensors.push_back(zeroPointTensor);

    // const_scale
    TosaSerializationOperator *scaleOp = nullptr;
    TosaSerializationTensor* scaleTensor = nullptr;
    CreateConstTosaOperator<float>(outputNameScale,
                                   scale,
                                   inputDType0,
                                   inputShape0,
                                   scaleOp,
                                   scaleTensor);
    tensors.push_back(scaleTensor);

    // mul
    int32_t shift = 0;
    TosaMulAttribute mulAttribute(shift);
    TosaSerializationOperator* mulOp = new TosaSerializationOperator(Op_MUL,
                                                                     Attribute_MulAttribute,
                                                                     &mulAttribute,
                                                                     {inputName, outputNameScale},
                                                                     {outputNameMul});
    tensors.push_back(new TosaSerializationTensor(outputNameMul, inputShape0, inputDType0, {}));

    // add
    TosaSerializationOperator* addOp = new TosaSerializationOperator(Op_ADD,
                                                                     Attribute_NONE,
                                                                     nullptr,
                                                                     {outputNameMul, outputNameZeroPoint},
                                                                     {outputNameAdd});
    tensors.push_back(new TosaSerializationTensor(outputNameAdd, inputShape0, inputDType0, {}));

    // cast
    TosaSerializationOperator* castOp = new TosaSerializationOperator(Op_CAST,
                                                                      Attribute_NONE,
                                                                      nullptr,
                                                                      {outputNameAdd},
                                                                      {outputName});

    tensors.push_back(new TosaSerializationTensor(outputName, outputShape0, outputDType0, {}));

    // operatorInputNames/operatorOutputNames ends up being the same as
    // blockInputNames/blockOutputNames for one-to-one ArmNN to TOSA mappings
    return new TosaSerializationBasicBlock(blockName,                                       // name
                                           mainName,                                        // region name
                                           {zeroPointOp, scaleOp, mulOp, addOp, castOp},    // operators
                                           tensors,                                         // tensors
                                           {inputName},                                     // inputs
                                           {outputName});                                   // outputs
}
