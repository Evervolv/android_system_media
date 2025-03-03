/*
 * Copyright (C) 2025 The Android Open Source Project
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

#define LOG_TAG "audio_dsp_tests"

#include <audio_utils/dsp_utils.h>

#include <gtest/gtest.h>
#include <utils/Log.h>

using namespace android::audio_utils;

/*
 * Check behavior on edge cases of 0 count or identical data.
 */
TEST(audio_dsp_tests, edge_cases) {
    constexpr float* noData{};
    std::vector<float> zeroData(10);
    std::vector<float> randomData(20);
    initUniformDistribution(randomData, -.2, .2);

    EXPECT_EQ(0, energy(noData, 0));
    EXPECT_EQ(std::numeric_limits<float>::infinity(), snr(noData, noData, 0));
    EXPECT_EQ(std::numeric_limits<float>::infinity(), snr(zeroData, zeroData));
    EXPECT_EQ(std::numeric_limits<float>::infinity(), snr(randomData, randomData));
}

/*
 * We use random energy tests to determine
 * whether the audio dsp methods works as expected.
 *
 * We avoid testing initUniform random number generator
 * for audio quality but rather suitability to evaluate
 * signal methods.
 */
TEST(audio_dsp_tests, random_energy) {
    constexpr size_t frameCount = 4096;
    constexpr size_t channelCount = 2;
    constexpr float amplitude = 0.1;
    constexpr size_t sampleCount = channelCount * frameCount;
    std::vector<float> randomData(sampleCount);
    initUniformDistribution(randomData, -amplitude, amplitude);

    // compute the expected energy in dB for a uniform distribution from -amplitude to amplitude.
    const float expectedEnergydB = energyOfUniformDistribution(-amplitude, amplitude);
    const float energy1dB = energy(randomData);
    ALOGD("%s: expectedEnergydB: %f  energy1dB: %f", __func__, expectedEnergydB, energy1dB);
    EXPECT_NEAR(energy1dB, expectedEnergydB, 0.1 /* epsilon */);  // within 0.1dB.

    std::vector<float> randomData2(sampleCount);
    initUniformDistribution(randomData2, -amplitude, amplitude);
    const float energy2dB = energy(randomData2);
    EXPECT_NEAR(energy1dB, energy2dB, 0.1);  // within 0.1dB.
    // data is correlated, see the larger epsilon.
    EXPECT_NEAR(-3, snr(randomData, randomData2), 2. /* epsilon */);

    std::vector<float> scaledData(sampleCount);
    constexpr float scale = 100.f;
    std::transform(randomData.begin(), randomData.end(), scaledData.begin(),
            [](auto e) { return e * scale; });
    const float energyScaled = energy(scaledData);
    const float scaledB = 20 * log10(scale);  // 40 = 20 log10(100).
    EXPECT_NEAR(scaledB, energyScaled - energy1dB, 1. /* epsilon */);
}

