//
// Copyright © 2023 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ParserFlatbuffersSerializeFixture.hpp"
#include <armnnDeserializer/IDeserializer.hpp>

#include <doctest/doctest.h>

#include <string>

TEST_SUITE("Deserializer_ReverseV2")
{
    struct ReverseV2Fixture : public ParserFlatbuffersSerializeFixture
    {
        explicit ReverseV2Fixture(const std::string& inputShape,
                                  const std::string& outputShape,
                                  const std::string& dataType,
                                  const std::string& axis)
        {
            m_JsonString = R"(
                {
                    inputIds: [0],
                    outputIds: [2],
                    layers: [
                        {
                            layer_type: "InputLayer",
                            layer: {
                                base: {
                                    layerBindingId: 0,
                                    base: {
                                        index: 0,
                                        layerName: "InputLayer",
                                        layerType: "Input",
                                        inputSlots: [{
                                            index: 0,
                                            connection: {sourceLayerIndex:0, outputSlotIndex:0 },
                                        }],
                                        outputSlots: [{
                                            index: 0,
                                            tensorInfo: {
                                                dimensions: )" + inputShape + R"(,
                                                dataType: )" + dataType + R"(
                                            }
                                        }]
                                    }
                                }
                            }
                        },
                        {
                            layer_type: "ReverseV2Layer",
                            layer: {
                                base: {
                                    index: 1,
                                    layerName: "ReverseV2Layer",
                                    layerType: "ReverseV2",
                                    inputSlots: [{
                                        index: 0,
                                        connection: {sourceLayerIndex:0, outputSlotIndex:0 },
                                    }],
                                    outputSlots: [{
                                        index: 0,
                                        tensorInfo: {
                                            dimensions: )" + outputShape + R"(,
                                            dataType: )" + dataType + R"(
                                        }
                                    }]
                                },
                                descriptor: {
                                    axis: )" + axis + R"(
                                }
                            }
                        },
                        {
                            layer_type: "OutputLayer",
                            layer: {
                                base:{
                                    layerBindingId: 2,
                                    base: {
                                        index: 2,
                                        layerName: "OutputLayer",
                                        layerType: "Output",
                                        inputSlots: [{
                                            index: 0,
                                            connection: {sourceLayerIndex:1, outputSlotIndex:0 },
                                        }],
                                        outputSlots: [{
                                            index: 0,
                                            tensorInfo: {
                                                dimensions: )" + outputShape + R"(,
                                                dataType: )" + dataType + R"(
                                            },
                                        }],
                                    }
                                }
                            }
                        }
                    ]
                }
            )";

            SetupSingleInputSingleOutput("InputLayer", "OutputLayer");
        }
    };

    // Test cases

    struct SimpleReverseV2FixtureFloat32 : ReverseV2Fixture
    {
        SimpleReverseV2FixtureFloat32()
            : ReverseV2Fixture("[ 2, 2 ]",
                               "[ 2, 2 ]",
                               "Float32",
                               "[1]")
        {}
    };

    TEST_CASE_FIXTURE(SimpleReverseV2FixtureFloat32, "SimpleReverseV2TestFloat32")
    {
        RunTest<4, armnn::DataType::Float32>(
            0,
            { 1.0f, 2.0f,
              3.0f, 4.0f },
            { 2.0f, 1.0f,
              4.0f, 3.0f }
            );
    }

    struct SimpleReverseV2FixtureFloat32ZeroAxis : ReverseV2Fixture
    {
        SimpleReverseV2FixtureFloat32ZeroAxis()
                : ReverseV2Fixture("[ 2, 2 ]",
                                   "[ 2, 2 ]",
                                   "Float32",
                                   "[0]")
        {}
    };

    TEST_CASE_FIXTURE(SimpleReverseV2FixtureFloat32ZeroAxis, "SimpleReverseV2TestFloat32ZeroAxis")
    {
        RunTest<4, armnn::DataType::Float32>(
                0,
                { 1.0f, 2.0f,
                  3.0f, 4.0f },
                { 3.0f, 4.0f,
                  1.0f, 2.0f }
        );
    }

    struct SimpleReverseV2FixtureFloat32NegativeAxis : ReverseV2Fixture
    {
        SimpleReverseV2FixtureFloat32NegativeAxis()
                : ReverseV2Fixture("[ 3, 3 ]",
                                   "[ 3, 3 ]",
                                   "Float32",
                                   "[-1]")
        {}
    };

    TEST_CASE_FIXTURE(SimpleReverseV2FixtureFloat32NegativeAxis, "SimpleReverseV2TestFloat32NegativeAxis")
    {
        RunTest<4, armnn::DataType::Float32>(
                0,
                { 1.0f, 2.0f, 3.0f,
                  4.0f, 5.0f, 6.0f,
                  7.0f, 8.0f, 9.0f },
                { 3.0f, 2.0f, 1.0f,
                  6.0f, 5.0f, 4.0f,
                  9.0f, 8.0f, 7.0f }
        );
    }

    struct SimpleReverseV2FixtureFloat32ThreeAxis : ReverseV2Fixture
    {
        SimpleReverseV2FixtureFloat32ThreeAxis()
                : ReverseV2Fixture("[ 3, 3, 3 ]",
                                   "[ 3, 3, 3 ]",
                                   "Float32",
                                   "[0, 2, 1]")
        {}
    };

    TEST_CASE_FIXTURE(SimpleReverseV2FixtureFloat32ThreeAxis, "SimpleReverseV2TestFloat32ThreeAxis")
    {
        RunTest<4, armnn::DataType::Float32>(
                0,
                { 1.0f, 2.0f, 3.0f,
                  4.0f, 5.0f, 6.0f,
                  7.0f, 8.0f, 9.0f,

                  11.0f, 12.0f, 13.0f,
                  14.0f, 15.0f, 16.0f,
                  17.0f, 18.0f, 19.0f,

                  21.0f, 22.0f, 23.0f,
                  24.0f, 25.0f, 26.0f,
                  27.0f, 28.0f, 29.0f },
                { 29.0f, 28.0f, 27.0f,
                  26.0f, 25.0f, 24.0f,
                  23.0f, 22.0f, 21.0f,

                  19.0f, 18.0f, 17.0f,
                  16.0f, 15.0f, 14.0f,
                  13.0f, 12.0f, 11.0f,

                  9.0f, 8.0f, 7.0f,
                  6.0f, 5.0f, 4.0f,
                  3.0f, 2.0f, 1.0f }
        );
    }

    struct SimpleReverseV2FixtureQuantisedAsymm8ThreeAxis : ReverseV2Fixture
    {
        SimpleReverseV2FixtureQuantisedAsymm8ThreeAxis()
                : ReverseV2Fixture("[ 3, 3, 3 ]",
                                   "[ 3, 3, 3 ]",
                                   "QuantisedAsymm8",
                                   "[0, 2, 1]")
        {}
    };

    TEST_CASE_FIXTURE(SimpleReverseV2FixtureQuantisedAsymm8ThreeAxis, "SimpleReverseV2TestQuantisedAsymm8ThreeAxis")
    {
        RunTest<4, armnn::DataType::QAsymmU8>(
                0,
                { 1, 2, 3,
                  4, 5, 6,
                  7, 8, 9,

                  11, 12, 13,
                  14, 15, 16,
                  17, 18, 19,

                  21, 22, 23,
                  24, 25, 26,
                  27, 28, 29 },
                { 29, 28, 27,
                  26, 25, 24,
                  23, 22, 21,

                  19, 18, 17,
                  16, 15, 14,
                  13, 12, 11,

                  9, 8, 7,
                  6, 5, 4,
                  3, 2, 1 }
        );
    }
}
