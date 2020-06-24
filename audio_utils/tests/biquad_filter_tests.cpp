/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <array>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <audio_utils/BiquadFilter.h>

using ::testing::Pointwise;
using ::testing::FloatNear;
using namespace android::audio_utils;

/************************************************************************************
 * Reference data, must not change.
 * The reference output data is from running in matlab y = filter(b, a, x), where
 *     b = [2.0f, 3.0f]
 *     a = [1.0f, 0.2f]
 *     x = [-0.1f, -0.2f, -0.3f, -0.4f, -0.5f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f]
 * The output y = [-0.2f, -0.66f, -1.068f, -1.4864f, -1.9027f,
 *                 -0.9195f, 0.8839f, 1.0232f, 1.4954f, 1.9009f].
 * The reference data construct the input and output as 2D array so that it can be
 * use to practice calling BiquadFilter::process multiple times.
 ************************************************************************************/
const size_t FRAME_COUNT = 5;
const size_t PERIOD = 2;
const float INPUT[PERIOD][FRAME_COUNT] = {
        {-0.1f, -0.2f, -0.3f, -0.4f, -0.5f},
        {0.1f, 0.2f, 0.3f, 0.4f, 0.5f}};
const std::array<float, kBiquadNumCoefs> COEFS = {
        2.0f, 3.0f, 0.0f, 0.2f, 0.0f };
const float OUTPUT[PERIOD][FRAME_COUNT] = {
        {-0.2f, -0.66f, -1.068f, -1.4864f, -1.9027f},
        {-0.9195f, 0.8839f, 1.0232f, 1.4954f, 1.9009f}};
const float EPS = 1e-4f;

class BiquadFilterTest : public ::testing::TestWithParam<size_t> {
protected:
    void populateBuffer(const float* singleChannelBuffer,
                        const size_t frameCount,
                        const size_t channelCount,
                        float* buffer);
};

void BiquadFilterTest::populateBuffer(const float *singleChannelBuffer,
                                      const size_t frameCount,
                                      const size_t channelCount,
                                      float *buffer) {
    for (size_t i = 0; i < frameCount; ++i) {
        for (size_t j = 0; j < channelCount; ++j) {
            buffer[i * channelCount + j] = singleChannelBuffer[i];
        }
    }
}

TEST_P(BiquadFilterTest, ConstructAndProcessFilter) {
    const size_t channelCount = static_cast<size_t>(GetParam());
    const size_t sampleCount = FRAME_COUNT * channelCount;
    float inputBuffer[PERIOD][sampleCount];
    float outputBuffer[sampleCount];
    float expectedOutputBuffer[PERIOD][sampleCount];
    for (size_t i = 0; i < PERIOD; ++i) {
        populateBuffer(INPUT[i], FRAME_COUNT, channelCount, inputBuffer[i]);
        populateBuffer(
                OUTPUT[i], FRAME_COUNT, channelCount, expectedOutputBuffer[i]);
    }
    BiquadFilter filter(channelCount, COEFS);

    for (size_t i = 0; i < PERIOD; ++i) {
        filter.process(outputBuffer, inputBuffer[i], FRAME_COUNT);
        EXPECT_THAT(std::vector<float>(outputBuffer, outputBuffer + sampleCount),
                    Pointwise(FloatNear(EPS), std::vector<float>(
                            expectedOutputBuffer[i], expectedOutputBuffer[i] + sampleCount)));
    }

    // After clear, the previous delays should be cleared.
    filter.clear();
    filter.process(outputBuffer, inputBuffer[0], FRAME_COUNT);
    EXPECT_THAT(std::vector<float>(outputBuffer, outputBuffer + sampleCount),
                Pointwise(FloatNear(EPS), std::vector<float>(
                        expectedOutputBuffer[0], expectedOutputBuffer[0] + sampleCount)));
}

INSTANTIATE_TEST_CASE_P(
        CstrAndRunBiquadFilter,
        BiquadFilterTest,
        ::testing::Values(1, 2)
        );
