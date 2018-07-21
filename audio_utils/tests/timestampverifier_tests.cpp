/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_timestampverifier_tests"

#include <stdio.h>

#include <audio_utils/TimestampVerifier.h>
#include <gtest/gtest.h>

// Ensure that all TimestampVerifier mutators are really constexpr and free from
// nasty system calls (in case called from a SCHED_FIFO thread).
static constexpr auto makeVerifier(
        size_t N, uint32_t sampleRate, size_t errors, size_t discontinuities) {
    android::TimestampVerifier<int64_t, int64_t> tv;

    int64_t f = 0;
    int64_t t = 0;
    for (size_t i = 0; i < N; ++i) {
        tv.add(f, t, sampleRate);
        f += sampleRate;
        t += (int64_t)1e9;
    }
    for (size_t i = 0; i < discontinuities; ++i) {
        tv.discontinuity();
    }
    for (size_t i = 0; i < errors; ++i) {
        tv.error();
    }
    return tv;
}

TEST(TimestampVerifier, sanity)
{
    constexpr android::TimestampVerifier<int64_t, int64_t> tv;
    constexpr android::audio_utils::Statistics<double> s = tv.getJitterMs();

    EXPECT_EQ(std::numeric_limits<double>::infinity(), s.getMin());
    EXPECT_EQ(-std::numeric_limits<double>::infinity(), s.getMax());

    constexpr int64_t frames[] { 0, 48000 };
    constexpr int64_t timeNs[] { 0, 1000000000 };
    constexpr android::TimestampVerifier<int64_t, int64_t> tv2(frames, timeNs, 48000);
    EXPECT_EQ(0., tv2.getJitterMs().getMax());
    EXPECT_EQ(0., tv2.getJitterMs().getMin());
    EXPECT_EQ(0., tv2.getJitterMs().getMean());

    constexpr android::TimestampVerifier<int64_t, int64_t> tv3 =
            makeVerifier(8 /* N */, 48000 /* sampleRate */, 10 /* errors */, 10 /* disc */);
    EXPECT_EQ(8, tv3.getN());
    EXPECT_EQ(10, tv3.getErrors());
    EXPECT_EQ(1, tv3.getDiscontinuities());  // consecutive discontinuities read as 1.
    EXPECT_EQ(0., tv3.getJitterMs().getMax());
    EXPECT_EQ(0., tv3.getJitterMs().getMin());
    EXPECT_EQ(0., tv3.getJitterMs().getMean());

    constexpr auto first = tv3.getFirstTimestamp();
    constexpr auto last = tv3.getLastTimestamp();

    EXPECT_EQ(0, first.first);
    EXPECT_EQ(0, first.second);
    EXPECT_EQ(48000 * (8 - 1), last.first);
    EXPECT_EQ((int64_t)1e9 * (8 - 1), last.second);
    EXPECT_EQ((uint32_t)48000, tv3.getSampleRate());
    EXPECT_EQ(0, tv3.getColds());
}
