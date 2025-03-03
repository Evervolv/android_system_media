/*
 * Copyright 2025 The Android Open Source Project
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

#pragma once

#ifdef __cplusplus

#include <algorithm>
#include <atomic>
#include <limits>
#include <random>
#include <type_traits>

namespace android::audio_utils {

/**
 * These DSP algorithms are intentionally designed for the typical audio use
 * case: single contiguous data layout.  This allows eventual vector
 * intrinsic optimization.
 *
 * Compare with STL generalized algorithm accumulate(), for_each[_n](),
 * or transform(), which use general ForwardIt forward iterators,
 * and composable ranges.
 */

/**
 * Fill a container with a uniform random distribution.
 *
 * The quality of the random number generator is tested
 * by audio_dsp_tests to be sufficient for basic signal
 * tests, not for audio noise or (shaped) dithering generation.
 */
static inline std::atomic<uint32_t> seedCounter = 1;  // random (resettable) seed.
template <typename T, typename V>
void initUniformDistribution(V& v, T rangeMin, T rangeMax) {
    // Fast but not great RNG.  Consider vectorized RNG in future.
    std::minstd_rand gen(++seedCounter);
    std::uniform_real_distribution<T> dis(rangeMin, rangeMax);

    for (auto& e : v) {
        e = dis(gen);
    }
}

/**
 * Return the energy in dB of a uniform distribution.
 */
template <typename F>
requires (std::is_floating_point_v<F>)
F energyOfUniformDistribution(F rangeMin, F rangeMax) {
    if (rangeMin == rangeMax) return 0;
    constexpr auto reciprocal = F(1) / 3;
    // (b^3 - a^3) / (b - a) = (b^2 + ab + a^2)
    return 10 * log10(reciprocal *
            (rangeMax * rangeMax + rangeMax * rangeMin + rangeMin * rangeMin));
}

/**
 * Compute the SNR in dB between two input arrays.
 *
 * The first array is considered the reference signal,
 * the second array is considered signal + noise.
 *
 * If in1 == in2, infinity is returned.
 * If count == 0, inifinty (not nan) is returned.
 */
template <typename F>
requires (std::is_floating_point_v<F>)
F snr(const F* in1, const F* in2, size_t count) {
    F signal{};
    F noise{};

    if (count == 0) return std::numeric_limits<F>::infinity();

    // floating point addition precision may depend on ordering.
    for (size_t i = 0; i < count; ++i) {
        signal += in1[i] * in1[i];
        const F diff = in1[i] - in2[i];
        noise += diff * diff;
    }

    if (noise == 0 && signal == 0) return std::numeric_limits<F>::infinity();
    return 10 * log10(signal / noise);
}

/**
 * Compute the SNR in dB between two input containers.
 *
 * The first container is considered the reference signal,
 * the second container is considered signal + noise.
 *
 * General container examples would be std::array, std::span, or std::vector.
 */
template <typename C>
auto snr(const C& c1, const C& c2) {
    return snr(c1.data(), c2.data(), std::min(c1.size(), c2.size()));
}

/**
 * Compute the energy (or power) in dB from an input array.
 *
 * Mean is not removed.
 *
 * This is a "square wave" reference dB measurement also known as dBov
 * (dB relative to overload).
 *
 * Audio standards typically use a full scale "sine wave" reference dB
 * measurement also known as dBFS.  With this terminology 0dBFS = -3dBov.
 *
 * If count == 0, 0 is returned.
 */
template <typename F>
requires (std::is_floating_point_v<F>)
F energy(const F* in, size_t count) {
    F signal{};

    if (count == 0) return 0;
    for (size_t i = 0; i < count; ++i) {
        signal += in[i] * in[i];
    }
    return 10 * log10(signal / count);
}

/**
 * Compute the energy (or power) in dB from an input container.
 *
 * Mean is not removed.
 *
 * This is a "square wave" reference dB measurement also known as dBov
 * (dB relative to overload).
 *
 * Audio standards typically use a full scale "sine wave" reference dB
 * measurement also known as dBFS.  With this terminology 0dBFS = -3dBov.
 *
 * General container examples would be std::array, std::span, or std::vector.
 */
template <typename C>
auto energy(const C& c) {
   return energy(c.data(), c.size());
}

}  // namespace android::audio_utils

#endif  // __cplusplus
