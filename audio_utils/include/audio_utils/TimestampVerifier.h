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

#ifndef ANDROID_AUDIO_UTILS_TIMESTAMP_VERIFIER_H
#define ANDROID_AUDIO_UTILS_TIMESTAMP_VERIFIER_H

#include <audio_utils/Statistics.h>

namespace android {

/** Verifies that a sequence of timestamps (a frame count, time pair)
 * is consistent based on sample rate.
 *
 * F is the type of frame counts (for example int64_t)
 * T is the type of time in Ns (for example int64_t ns)
 *
 * All methods except for toString() are safe to call from a SCHED_FIFO thread.
 */
template <typename F /* frame count */, typename T /* time units */>
class TimestampVerifier {
public:
    constexpr TimestampVerifier() = default;

    // construct from static arrays.
    template <size_t N>
    constexpr TimestampVerifier(const F (&frames)[N], const T (&timeNs)[N], uint32_t sampleRate) {
         for (size_t i = 0; i < N; ++i) {
             add(frames[i], timeNs[i], sampleRate);
         }
    }

    /** adds a timestamp, represented by a (frames, timeNs) pair and the
     * sample rate to the TimestampVerifier.
     *
     * The frames and timeNs should be monotonic increasing
     * (from the previous discontinuity, or the start of adding).
     *
     * A sample rate change triggers a discontinuity automatically.
     */
    constexpr void add(F frames, T timeNs, uint32_t sampleRate) {
        // We consider negative time as "not ready".
        // TODO: do we need to consider an explicit epoch start time?
        if (timeNs < 0) {
           ++mNotReady;
           return;
        }
        if (mDiscontinuity || mSampleRate != sampleRate) {
            mDiscontinuity = false;
            copyTo(mFirstTimestamp, {frames, timeNs});
            copyTo(mLastTimestamp, mFirstTimestamp);
            mSampleRate = sampleRate;
        } else {
            assert(sampleRate != 0);
            const FrameTime timestamp{frames, timeNs};
            mJitterMs.add(computeJitterMs(timestamp, mLastTimestamp, sampleRate));
            copyTo(mLastTimestamp, timestamp);
        }
        ++mTimestamps;
    }

    /** registers a discontinuity.
     *
     * The next timestamp added does not participate in any statistics with the last
     * timestamp, but rather anchors following timestamp sequence verification.
     *
     * Consecutive discontinuities are treated as one for the purposes of counting.
     */
    constexpr void discontinuity() {
        if (!mDiscontinuity) {
            mDiscontinuity = true;
            ++mDiscontinuities;
        }
    }

    /** registers an error.
     *
     * The timestamp sequence is still assumed continuous after error. Use discontinuity()
     * if it is not.
     */
    constexpr void error() {
        ++mErrors;
    }

    /** returns a string with relevant statistics.
     *
     * Should not be called from a SCHED_FIFO thread since it uses std::string.
     */
    std::string toString() const {
        std::stringstream ss;

        ss << "n=" << mTimestamps;          // number of timestamps added with valid times.
        ss << " disc=" << mDiscontinuities; // discontinuities encountered (dups ignored).
        ss << " nRdy=" << mNotReady;        // timestamps not ready (time negative).
        ss << " err=" << mErrors;           // errors encountered.
        if (mSampleRate != 0) {             // ratio of time-by-frames / time
            ss << " rate=" << computeRatio( // (since last discontinuity).
                    mLastTimestamp, mFirstTimestamp, mSampleRate);
        }
        ss << " jitterMs(" << mJitterMs.toString() << ")";  // timestamp jitter statistics.
        return ss.str();
    }

    // general counters
    constexpr int64_t getN() const { return mTimestamps; }
    constexpr int64_t getDiscontinuities() const { return mDiscontinuities; }
    constexpr int64_t getNotReady() const { return mNotReady; }
    constexpr int64_t getErrors() const { return mErrors; }
    constexpr const Statistics<double> & getJitterMs() const { return mJitterMs; }

    // timestamp anchor info
    using FrameTime = std::pair<F, T>;
    constexpr FrameTime getFirstTimestamp() const { return mFirstTimestamp; }
    constexpr FrameTime getLastTimestamp() const { return mLastTimestamp; }
    constexpr uint32_t getSampleRate() const { return mSampleRate; }

private:

    // general counters
    int64_t mTimestamps = 0;
    int64_t mDiscontinuities = 0;
    int64_t mNotReady = 0;
    int64_t mErrors = 0;
    Statistics<double> mJitterMs{0.999}; // weight more recent history higher.

    // timestamp anchor info
    bool mDiscontinuity = true;
    FrameTime mFirstTimestamp;
    FrameTime mLastTimestamp;
    uint32_t mSampleRate = 0;

    // TODO: remove when std::pair has a constexpr copy operator.
    static void constexpr copyTo(FrameTime& to, const FrameTime &from) {
       to.first = from.first;
       to.second = from.second;
    }

    // sub returns the signed type of the difference between left and right.
    // This is only important if F or T are unsigned int types.
    __attribute__((no_sanitize("integer")))
    static constexpr auto sub(const FrameTime &left, const FrameTime &right) {
        return std::make_pair<
                typename std::make_signed<F>::type, typename std::make_signed<T>::type>(
                        left.first - right.first, left.second - right.second);
    }

    // Inf+-, NaN is possible only if sampleRate is 0 (should not happen)
    static constexpr double computeJitterMs(
            const FrameTime &current, const FrameTime &last, uint32_t sampleRate) {
        const auto diff = sub(current, last);
        const double frameDifferenceNs = diff.first * 1e9 / sampleRate;
        const double jitterNs = frameDifferenceNs - diff.second;
        return jitterNs * 1e-6;
    }

    // Inf+-, Nan possible depending on differences between current and last.
    static constexpr double computeRatio(
            const FrameTime &current, const FrameTime &last, uint32_t sampleRate) {
        const auto diff = sub(current, last);
        const double frameDifferenceNs = diff.first * 1e9 / sampleRate;
        return frameDifferenceNs / diff.second;
    }
};

} // namespace android

#endif // !ANDROID_AUDIO_UTILS_TIMESTAMP_VERIFIER_H
