/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 *
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIO_UTILS_BIQUAD_FILTER_H
#define ANDROID_AUDIO_UTILS_BIQUAD_FILTER_H

#include <array>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include <assert.h>

namespace android::audio_utils {

static constexpr size_t kBiquadNumCoefs  = 5;
static constexpr size_t kBiquadNumDelays = 2;

namespace details {
// Helper methods for constructing a constexpr array of function pointers.
// As function pointers are efficient and have no constructor/destructor
// this is preferred over std::function.

template <template <size_t, typename ...> typename F, size_t... Is>
static inline constexpr auto make_functional_array_from_index_sequence(std::index_sequence<Is...>) {
    using first_t = decltype(&F<0>::func);  // type from function
    using result_t = std::array<first_t, sizeof...(Is)>;   // type of array
    return result_t{{F<Is>::func...}};      // initialize with functions.
}

template <template <size_t, typename ...> typename F, size_t M>
static inline constexpr auto make_functional_array() {
    return make_functional_array_from_index_sequence<F>(std::make_index_sequence<M>());
}

// For biquad_filter_fast, we template based on whether coef[i] is nonzero - this should be
// determined in a constexpr fashion for optimization.
template <size_t OCCUPANCY>
void biquad_filter_fast(const float *coefs, const float *in, size_t channelCount,
                        size_t frames, float *delays, float *out) {
    const float b0 = coefs[0];
    const float b1 = coefs[1];
    const float b2 = coefs[2];
    const float negativeA1 = -coefs[3];
    const float negativeA2 = -coefs[4];
#if defined(__i386__) || defined(__x86_x64__)
    float delta = std::numeric_limits<float>::min() * (1 << 24);
#endif
    for (size_t i = 0; i < channelCount; ++i) {
        float s1n1 = delays[i * kBiquadNumDelays];
        float s2n1 = delays[i * kBiquadNumDelays + 1];
        const float *input = &in[i];
        float *output = &out[i];
        for (size_t j = frames; j > 0; --j) {
            // Adding a delta to avoid high latency with subnormal data. The high latency is not
            // significant with ARM platform, but on x86 platform. The delta will not affect the
            // precision of the result.
#if defined(__i386__) || defined(__x86_x64__)
            const float xn = *input + delta;
#else
            const float xn = *input;
#endif
            float yn = (OCCUPANCY >> 0 & 1) * b0 * xn + s1n1;
            s1n1 = (OCCUPANCY >> 1 & 1) * b1 * xn + (OCCUPANCY >> 3 & 1) * negativeA1 * yn + s2n1;
            s2n1 = (OCCUPANCY >> 2 & 1) * b2 * xn + (OCCUPANCY >> 4 & 1) * negativeA2 * yn;

            input += channelCount;

            *output = yn;
            output += channelCount;

#if defined(__i386__) || defined(__x86_x64__)
            delta = -delta;
#endif
        }
        delays[i * kBiquadNumDelays] = s1n1;
        delays[i * kBiquadNumDelays + 1] = s2n1;
    }
}

} // namespace details

// When using BiquadFilter, `-ffast-math` is needed to get non-zero optimization.
// TODO(b/159373530): Use compound statement scoped pragmas so that the library
// users not need to add `-ffast-math`.
class BiquadFilter {
public:
    template <typename T = std::array<float,kBiquadNumCoefs>>
    explicit BiquadFilter(const size_t channelCount, const T& coefs = {})
            : mChannelCount(channelCount), mDelays(channelCount * kBiquadNumDelays) {
        setCoefficients(coefs);
    }

    /**
     * \brief Sets filter coefficients
     *
     * \param coefs  pointer to the filter coefficients array
     * \return true if the BiquadFilter is stable. Otherwise, return false.
     *
     * The coefficients are interpreted in the following manner:
     * coefs[0] is b0, coefs[1] is b1,
     * coefs[2] is b2, coefs[3] is a1,
     * coefs[4] is a2.
     *
     * coefs are used in Biquad filter equation as follows:
     * y[n] = b0 * x[n] + s1[n - 1]
     * s1[n] = s2[n - 1] + b1 * x[n] - a1 * y[n]
     * s2[n] = b2 * x[n] - a2 * y[n]
     */
    template <typename T = std::array<float,kBiquadNumCoefs>>
    bool setCoefficients(const T& coefs) {
        assert(coefs.size() == kBiquadNumCoefs);
        bool stable = (fabs(coefs[4]) < 1.0f && fabs(coefs[3]) < 1.0f + coefs[4]);
        // Determine which coefficients are nonzero.
        size_t category = 0;
        for (size_t i = 0; i < kBiquadNumCoefs; ++i) {
            category |= (coefs[i] != 0) << i;
        }

        // Select the proper filtering function from our array.
        mFunc = mArray[category];
        std::copy(coefs.begin(), coefs.end(), mCoefs.begin());
        return stable;
    }

    /**
     * \brief Filters the input data
     *
     * Carries out filtering on the input data.
     *
     * \param out     pointer to the output data
     * \param in      pointer to the input data
     * \param frames  number of audio frames to be processed
     */
    void process(float* out, const float *in, const size_t frames) {
        mFunc(mCoefs.data(), in, mChannelCount, frames, mDelays.data(), out);
    }

    /**
     * \brief Clears the input and output data
     *
     * This function clears the input and output delay elements
     * stored in delay buffer.
     */
    void clear() {
        std::fill(mDelays.begin(), mDelays.end(), 0.f);
    }

    /**
     * \brief Sets mDelays with the values passed.
     *
     * \param delays    reference to vector containing delays.
     */
    void setDelays(std::vector<float>& delays) {
        assert(delays.size() == mDelays.size());
        mDelays = std::move(delays);
    }

    /**
     * \brief Gets delays vector with delay values at mDelays vector.
     *
     * \return a const vector reference of delays.
     */
    const std::vector<float>& getDelays() {
        return mDelays;
    }

private:
    const size_t mChannelCount;

    /*
     * \var float mCoefs
     * \brief Stores the filter coefficients
     */
    std::array<float, kBiquadNumCoefs> mCoefs;

    /**
     * \var float mDelays
     * \brief Stores the input and outpt delays.
     *
     * The delays are stored in the following manner,
     * mDelays[2 * i + 0] is s1(n-1) of i-th channel
     * mDelays[2 * i + 1] is s2(n-1) of i-th channel
     * index i runs from 0 to (mChannelCount - 1).
     */
    std::vector<float> mDelays;

    // Declare a float filter function.
    using float_filter_func = decltype(details::biquad_filter_fast<0>);

    float_filter_func *mFunc;

    // Create a functional wrapper to feed "biquad_filter_fast" to
    // make_functional_array() to populate the array.
    template <size_t OCCUPANCY>
    struct FuncWrap {
        static void func(const float *coef, const float *in, const size_t channelCount,
                         const size_t frames, float *delays, float* out) {
            details::biquad_filter_fast<OCCUPANCY>(coef, in, channelCount, frames, delays, out);
        }
    };

    // Create the std::array of functions.
    //   static inline constexpr std::array<float_filter_func*, M> mArray = {
    //     biquad_filter_fast<0>,
    //     biquad_filter_fast<1>,
    //     biquad_filter_fast<2>,
    //      ...
    //     biquad_filter_fast<(1 << kBiquadNumCoefs) - 1>,
    //  };
    // The actual processing function will be picked based on the coefficients.
    static inline constexpr auto mArray =
            details::make_functional_array<FuncWrap, 1 << kBiquadNumCoefs>();
};

} // namespace android::audio_utils

#endif  // !ANDROID_AUDIO_UTILS_BIQUAD_FILTER_H
