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
//
// SC stands for SAME_COEF_PER_CHANNEL, a compile time boolean constant.
template <template <size_t, bool, typename ...> typename F, bool SC, size_t... Is>
static inline constexpr auto make_functional_array_from_index_sequence(std::index_sequence<Is...>) {
    using first_t = decltype(&F<0, false>::func);  // type from function
    using result_t = std::array<first_t, sizeof...(Is)>;   // type of array
    return result_t{{F<Is, SC>::func...}};      // initialize with functions.
}

template <template <size_t, bool, typename ...> typename F, size_t M, bool SC>
static inline constexpr auto make_functional_array() {
    return make_functional_array_from_index_sequence<F, SC>(std::make_index_sequence<M>());
}

// Returns true if the poles are stable for a Biquad.
template <typename D>
static inline constexpr bool isStable(const D& a1, const D& a2) {
    return fabs(a2) < D(1) && fabs(a1) < D(1) + a2;
}

// Simplifies Biquad coefficients.
// TODO: consider matched pole/zero cancellation.
//       consider subnormal elimination for Intel processors.
template <typename D, typename T>
std::array<D, kBiquadNumCoefs> reduceCoefficients(const T& coef) {
    std::array<D, kBiquadNumCoefs> lcoef;
    if (coef.size() == kBiquadNumCoefs + 1) {
        // General form of Biquad.
        // Remove matched z^-1 factors in top and bottom (e.g. coefs[0] == coefs[3] == 0).
        size_t offset = 0;
        for (; offset < 2 && coef[offset] == 0 && coef[offset + 3] == 0; ++offset);
        assert(coefs[offset + 3] != 0); // hmm... shouldn't we be causal?

        // Normalize 6 coefficients to 5 for storage.
        lcoef[0] = coef[offset] / coef[offset + 3];
        for (size_t i = 1; i + offset < 3; ++i) {
            lcoef[i] = coef[i + offset] / coef[offset + 3];
            lcoef[i + 2] = coef[i + offset + 3] / coef[offset + 3];
         }
    } else if (coef.size() == kBiquadNumCoefs) {
        std::copy(coef.begin(), coef.end(), lcoef.begin());
    } else {
        assert(coef.size() == kBiquadNumCoefs + 1 || coef.size() == kBiquadNumCoefs);
    }
    return lcoef;
}

// Sets a container of coefficients to storage.
template <typename D, typename T, typename DEST>
static inline void setCoefficients(
        DEST& dest, size_t offset, size_t stride, size_t channelCount, const T& coef) {
    auto lcoef = reduceCoefficients<D, T>(coef);
    // replicate as needed
    for (size_t i = 0; i < kBiquadNumCoefs; ++i) {
        for (size_t j = 0; j < channelCount; ++j) {
            dest[i * stride + offset + j] = lcoef[i];
        }
    }
}

// For biquad_filter_fast, we template based on whether coef[i] is nonzero - this should be
// determined in a constexpr fashion for optimization.

// Helper which takes a stride to allow column processing of interleaved audio streams.
template <size_t OCCUPANCY, bool SAME_COEF_PER_CHANNEL, typename D>
void biquad_filter_1fast(D *out, const D *in, size_t frames, size_t stride,
                         size_t channelCount, D *delays, const D *coefs, size_t localStride) {
#if defined(__i386__) || defined(__x86_x64__)
    D delta = std::numeric_limits<float>::min() * (1 << 24);
#endif
    D b0, b1, b2, negativeA1, negativeA2;

    if constexpr (SAME_COEF_PER_CHANNEL) {
        b0 = coefs[0];
        b1 = coefs[1];
        b2 = coefs[2];
        negativeA1 = -coefs[3];
        negativeA2 = -coefs[4];
    }
    for (size_t i = 0; i < channelCount; ++i) {
        if constexpr (!SAME_COEF_PER_CHANNEL) {
            b0 = coefs[0];
            b1 = coefs[localStride];
            b2 = coefs[2 * localStride];
            negativeA1 = -coefs[3 * localStride];
            negativeA2 = -coefs[4 * localStride];
            ++coefs;
        }

        D s1n1 = delays[0];
        D s2n1 = delays[localStride];
        const D *input = &in[i];
        D *output = &out[i];
        for (size_t j = frames; j > 0; --j) {
            // Adding a delta to avoid subnormal exception handling on the x86/x64 platform;
            // this is not a problem with the ARM platform. The delta will not affect the
            // precision of the result.
#if defined(__i386__) || defined(__x86_x64__)
            const D xn = *input + delta;
#else
            const D xn = *input;
#endif
            D yn = (OCCUPANCY >> 0 & 1) * b0 * xn + s1n1;
            s1n1 = (OCCUPANCY >> 1 & 1) * b1 * xn + (OCCUPANCY >> 3 & 1) * negativeA1 * yn + s2n1;
            s2n1 = (OCCUPANCY >> 2 & 1) * b2 * xn + (OCCUPANCY >> 4 & 1) * negativeA2 * yn;

            input += stride;

            *output = yn;
            output += stride;

#if defined(__i386__) || defined(__x86_x64__)
            delta = -delta;
#endif
        }
        delays[0] = s1n1;
        delays[localStride] = s2n1;
        ++delays;
    }
}

// Helper function to zero channels in the input buffer.
// This is used for the degenerate coefficient case which results in all zeroes.
template <typename D>
void zeroChannels(D *out, size_t frames, size_t stride, size_t channelCount) {
    if (stride == channelCount) {
        memset(out, 0, sizeof(float) * frames * channelCount);
    } else {
        for (size_t i = 0; i < frames; i++) {
            memset(out, 0, sizeof(float) * channelCount);
            out += stride;
        }
    }
}

template <size_t OCCUPANCY, bool SAME_COEF_PER_CHANNEL, typename D>
void biquad_filter_fast(D *out, const D *in, size_t frames, size_t stride,
        size_t channelCount, D *delays, const D *coefs, size_t localStride) {
    if constexpr ((OCCUPANCY & 7) == 0) { // all b's are zero, output is zero.
        zeroChannels(out, frames, stride, channelCount);
        return;
    }
    biquad_filter_1fast<OCCUPANCY, SAME_COEF_PER_CHANNEL>(
            out, in, frames, stride, channelCount, delays, coefs, localStride);
}

} // namespace details

/**
 * BiquadFilter
 *
 * A multichannel Biquad filter implementation of the following transfer function.
 *
 * \f[
 *  H(z) = \frac { b_0 + b_1 z^{-1} + b_2 z^{-2} }
 *               { 1   + a_1 z^{-1} + a_2 z^{-2} }
 * \f]
 *
 * <!--
 *        b_0 + b_1 z^{-1} + b_2 z^{-2}
 *  H(z)= -----------------------------
 *        1 + a_1 z^{-1} + a_2 z^{-2}
 * -->
 *
 *  Details:
 *    1. The transposed direct type 2 implementation allows zeros to be computed
 *       before poles in the internal state for improved filter precision and
 *       better time-varying coefficient performance.
 *    2. We optimize for zero coefficients using a compile-time generated function table.
 *    3. We optimize for vector operations using column vector operations with stride
 *       into interleaved audio data.
 *    4. The denominator coefficients a_1 and a_2 are stored in positive form, despite the
 *       negated form being slightly simpler for optimization (addition is commutative but
 *       subtraction is not commutative).  This is to permit obtaining the coefficients
 *       as a const reference.
 *
 *       Compatibility issue: Some Biquad libraries store the denominator coefficients
 *       in negated form.  We explicitly negate before entering into the inner loop.
 *    5. The full 6 coefficient Biquad filter form with a_0 != 1 may be used for setting
 *       coefficients.  See setCoefficients() below.
 *
 * If SAME_COEFFICIENTS_PER_CHANNEL is false, then mCoefs is stored interleaved by channel.
 *
 * The Biquad filter update equation in transposed Direct form 2 is as follows:
 *
 * \f{eqnarray*}{
 * y[n] &=& b0 * x[n] + s1[n - 1] \\
 * s1[n] &=& s2[n - 1] + b1 * x[n] - a1 * y[n] \\
 * s2[n] &=& b2 * x[n] - a2 * y[n]
 * \f}
 *
 * For the transposed Direct form 2 update equation s1 and s2 represent the delay state
 * contained in the internal vector mDelays[].  This is stored interleaved by channel.
 *
 * Use -ffast-math` to permit associative math optimizations to get non-zero optimization as
 * we do not rely on strict C operator precedence and associativity here.
 * TODO(b/159373530): Use compound statement scoped pragmas instead of `-ffast-math`.
 *
 * \param D type variable representing the data type, one of float or double.
 *         The default is float.
 * \param SAME_COEF_PER_CHANNEL bool which is true if all the Biquad coefficients
 *         are shared between channels, or false if the Biquad coefficients
 *         may differ between channels. The default is true.
 */
template <typename D = float, bool SAME_COEF_PER_CHANNEL = true>
class BiquadFilter {
public:
    template <typename T = std::array<D, kBiquadNumCoefs>>
    explicit BiquadFilter(size_t channelCount,
            const T& coefs = {})
            : mChannelCount(channelCount)
            , mCoefs(kBiquadNumCoefs * (SAME_COEF_PER_CHANNEL ? 1 : mChannelCount))
            , mDelays(channelCount * kBiquadNumDelays) {
        setCoefficients(coefs);
    }

    // copy constructors
    BiquadFilter(const BiquadFilter<D, SAME_COEF_PER_CHANNEL>& other) {
        *this = other;
    }

    BiquadFilter(BiquadFilter<D, SAME_COEF_PER_CHANNEL>&& other) {
        *this = std::move(other);
    }

    // copy assignment
    BiquadFilter<D, SAME_COEF_PER_CHANNEL>& operator=(
            const BiquadFilter<D, SAME_COEF_PER_CHANNEL>& other) {
        mChannelCount = other.mChannelCount;
        mCoefs = other.mCoefs;
        mDelays = other.mDelays;
        return *this;
    }

    BiquadFilter<D, SAME_COEF_PER_CHANNEL>& operator=(
            BiquadFilter<D, SAME_COEF_PER_CHANNEL>&& other) {
        mChannelCount = other.mChannelCount;
        mCoefs = std::move(other.mCoefs);
        mDelays = std::move(other.mDelays);
        return *this;
    }

    // operator overloads for equality tests
    bool operator==(const BiquadFilter<D, SAME_COEF_PER_CHANNEL>& other) const {
        return mChannelCount == other.mChannelCount
                && mCoefs == other.mCoefs
                && mDelays == other.mDelays;
    }

    bool operator!=(const BiquadFilter<D, SAME_COEF_PER_CHANNEL>& other) const {
        return !operator==(other);
    }

    /**
     * \brief Sets filter coefficients
     *
     * \param coefs  pointer to the filter coefficients array.
     * \return true if the BiquadFilter is stable, otherwise, return false.
     *
     * The input coefficients are interpreted in the following manner:
     *
     * If size of container is 5 (normalized Biquad):
     * coefs[0] is b0,
     * coefs[1] is b1,
     * coefs[2] is b2,
     * coefs[3] is a1,
     * coefs[4] is a2.
     *
     * \f[
     *  H(z) = \frac { b_0 + b_1 z^{-1} + b_2 z^{-2} }
     *               { 1   + a_1 z^{-1} + a_2 z^{-2} }
     * \f]
     * <!--
     *        b_0 + b_1 z^{-1} + b_2 z^{-2}
     *  H(z)= -----------------------------
     *        1 + a_1 z^{-1} + a_2 z^{-2}
     * -->
     *
     * If size of container is 6 (general Biquad):
     * coefs[0] is b0,
     * coefs[1] is b1,
     * coefs[2] is b2,
     * coefs[3] is a0,
     * coefs[4] is a1,
     * coefs[5] is a2.
     *
     * \f[
     *  H(z) = \frac { b_0 + b_1 z^{-1} + b_2 z^{-2} }
     *               { a_0 + a_1 z^{-1} + a_2 z^{-2} }
     * \f]
     * <!--
     *        b_0 + b_1 z^{-1} + b_2 z^{-2}
     *  H(z)= -----------------------------
     *        a_0 + a_1 z^{-1} + a_2 z^{-2}
     * -->
     *
     * The internal representation is a normalized Biquad.
     */
    template <typename T = std::array<D, kBiquadNumCoefs>>
    bool setCoefficients(const T& coefs) {
        if constexpr (SAME_COEF_PER_CHANNEL) {
            details::setCoefficients<D, T>(
                    mCoefs, 0 /* offset */, 1 /* stride */, 1 /* channelCount */, coefs);
        } else {
            if (coefs.size() == mCoefs.size()) {
                std::copy(coefs.begin(), coefs.end(), mCoefs.begin());
            } else {
                details::setCoefficients<D, T>(
                        mCoefs, 0 /* offset */, mChannelCount, mChannelCount, coefs);
            }
        }
        setOptimization();
        return isStable();
    }

    /**
     * Sets coefficients for one of the filter channels, specified by channelIndex.
     *
     * This method is only available if SAME_COEF_PER_CHANNEL is false.
     *
     * \param coefs the coefficients to set.
     * \param channelIndex the particular channel index to set.
     */
    template <typename T = std::array<D, kBiquadNumCoefs>>
    bool setCoefficients(const T& coefs, size_t channelIndex) {
        static_assert(!SAME_COEF_PER_CHANNEL);

        details::setCoefficients<D, T>(
                mCoefs, channelIndex, mChannelCount, 1 /* channelCount */, coefs);
        setOptimization();
        return isStable();
    }

    /**
     * Returns the coefficients as a const vector reference.
     *
     * If multichannel and the template variable SAME_COEF_PER_CHANNEL is true,
     * the coefficients are interleaved by channel.
     */
    const std::vector<D>& getCoefficients() const {
        return mCoefs;
    }

    /**
     * Returns true if the filter is stable.
     *
     * \param channelIndex ignored if SAME_COEF_PER_CHANNEL is true,
     *        asserts if channelIndex >= channel count (zero based index).
     */
    bool isStable(size_t channelIndex = 0) const {
        if constexpr (SAME_COEF_PER_CHANNEL) {
            return details::isStable(mCoefs[3], mCoefs[4]);
        } else {
            assert(channelIndex < mChannelCount);
            return details::isStable(
                    mCoefs[3 * mChannelCount + channelIndex],
                    mCoefs[4 * mChannelCount + channelIndex]);
        }
    }

    /**
     * Updates the filter function based on processor optimization.
     *
     */
    void setOptimization() {
        // Determine which coefficients are nonzero as a bit field.
        size_t category = 0;
        for (size_t i = 0; i < kBiquadNumCoefs; ++i) {
            if constexpr (SAME_COEF_PER_CHANNEL) {
                category |= (mCoefs[i] != 0) << i;
            } else {
                for (size_t j = 0; j < mChannelCount; ++j) {
                    if (mCoefs[i * mChannelCount + j] != 0) {
                        category |= 1 << i;
                        break;
                    }
                }
            }
        }

        // Select the proper filtering function from our array.
        mFunc = mFilterFast[category];  // default if we don't have processor optimization.

    }

    /**
     * \brief Filters the input data
     *
     * \param out     pointer to the output data
     * \param in      pointer to the input data
     * \param frames  number of audio frames to be processed
     */
    void process(D* out, const D *in, size_t frames) {
        process(out, in, frames, mChannelCount);
    }

    /**
     * \brief Filters the input data with stride
     *
     * \param out     pointer to the output data
     * \param in      pointer to the input data
     * \param frames  number of audio frames to be processed
     * \param stride  the total number of samples associated with a frame, if not channelCount.
     */
    void process(D* out, const D *in, size_t frames, size_t stride) {
        assert(stride >= mChannelCount);
        mFunc(out, in, frames, stride, mChannelCount, mDelays.data(),
                mCoefs.data(), mChannelCount);
    }

    /**
     * \brief Clears the delay elements
     *
     * This function clears the delay elements representing the filter state.
     */
    void clear() {
        std::fill(mDelays.begin(), mDelays.end(), 0.f);
    }

    /**
     * \brief Sets the internal delays from a vector
     *
     * For a multichannel stream, the delays are interleaved by channel:
     * delays[2 * i + 0] is s1 of i-th channel,
     * delays[2 * i + 1] is s2 of i-th channel,
     * where index i runs from 0 to (mChannelCount - 1).
     *
     * \param delays reference to vector containing delays.
     */
    void setDelays(std::vector<D>& delays) {
        assert(delays.size() == mDelays.size());
        mDelays = std::move(delays);
    }

    /**
     * \brief Gets delay elements as a vector
     *
     * For a multichannel stream, the delays are interleaved by channel:
     * delays[2 * i + 0] is s1 of i-th channel,
     * delays[2 * i + 1] is s2 of i-th channel,
     * where index i runs from 0 to (mChannelCount - 1).
     *
     * \return a const vector reference of delays.
     */
    const std::vector<D>& getDelays() const {
        return mDelays;
    }

private:
    /* const */ size_t mChannelCount; // not const because we can assign to it on operator equals.

    /*
     * \var D mCoefs
     * \brief Stores the filter coefficients
     *
     * If SAME_COEF_PER_CHANNEL is false, the filter coefficients are stored
     * interleaved by channel.
     */
    std::vector<D> mCoefs;

    /**
     * \var D mDelays
     * \brief The delay state.
     *
     * The delays are stored channel interleaved in the following manner,
     * mDelays[2 * i + 0] is s1 of i-th channel
     * mDelays[2 * i + 1] is s2 of i-th channel
     * index i runs from 0 to (mChannelCount - 1).
     */
    std::vector<D> mDelays;

    using filter_func = decltype(details::biquad_filter_fast<0, true, D>);

    /**
     * \var filter_func* mFunc
     *
     * The current filter function selected for the channel occupancy of the Biquad.
     */
    filter_func *mFunc;

    // Create a functional wrapper to feed "biquad_filter_fast" to
    // make_functional_array() to populate the array.
    template <size_t OCCUPANCY, bool SC> // note SC == SAME_COEF_PER_CHANNEL
    struct FuncWrap {
        template<typename T>
        static constexpr size_t nearest() {
            // Combine cases to both improve expected performance and reduce code space.
            // Some occupancy masks provide worse performance than more occupied masks.
            constexpr size_t required_occupancies[] = {
                1,  // constant scale
                3,  // single zero
                7,  // double zero
                9,  // single pole
                // 11, // first order IIR (unnecessary optimization, close enough to 31).
                27, // double pole + single zero
                31, // second order IIR (full Biquad)
            };
            if constexpr (OCCUPANCY < 32) {
                for (auto test : required_occupancies) {
                    if ((OCCUPANCY & test) == OCCUPANCY) return test;
                }
            }
            return 0; // never gets here.
        }

        static void func(D* out, const D *in, size_t frames, size_t stride,
                size_t channelCount, D *delays, const D *coef, size_t localStride) {
            constexpr size_t NEAREST_OCCUPANCY = nearest<D>();
            details::biquad_filter_fast<NEAREST_OCCUPANCY, SC>(
                    out, in, frames, stride, channelCount, delays, coef, localStride);
        }
    };

    /**
     * \var mFilterFast
     *
     * std::array of functions based on coefficient occupancy.
     *
     *  static inline constexpr std::array<filter_func*, M> mArray = {
     *     biquad_filter_fast<0>,
     *     biquad_filter_fast<1>,
     *     biquad_filter_fast<2>,
     *      ...
     *     biquad_filter_fast<(1 << kBiquadNumCoefs) - 1>,
     *  };
     *
     * Every time the coefficients are changed, we select the processing function from
     * this table.
     */
    static inline constexpr auto mFilterFast =
            details::make_functional_array<
                    FuncWrap, 1 << kBiquadNumCoefs, SAME_COEF_PER_CHANNEL>();

};

} // namespace android::audio_utils

#endif  // !ANDROID_AUDIO_UTILS_BIQUAD_FILTER_H
