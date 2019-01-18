/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_UTILS_BALANCE_H
#define ANDROID_AUDIO_UTILS_BALANCE_H

#include <math.h>       /* exp */
#include <sstream>
#include <system/audio.h>
#include <vector>

namespace android::audio_utils {

class Balance {
public:
    Balance();

    /**
     * \brief Sets the channel mask for data passed in.
     *
     * \param channelMask The audio output channel mask to use.
     *                    Invalid channel masks are ignored.
     *
     */
    void setChannelMask(audio_channel_mask_t channelMask);

    /**
     * \brief Processes balance for audio data.
     *
     * \param buffer    pointer to the audio data to be modified in-place.
     * \param balance   from -1.f (left) to 0.f (center) to 1.f (right)
     * \param frames    numer of frames of audio data to convert.
     *
     */
    void process(float *buffer, float balance, size_t frames);

    /**
     * \brief Computes the stereo gains for left and right channels.
     *
     * \param balance   from -1.f (left) to 0.f (center) to 1.f (right)
     * \param left      pointer to the float where the left gain will be stored.
     * \param right     pointer to the float where the right gain will be stored.
     */
    void computeStereoBalance(float balance, float *left, float *right) const;

    /**
     * \brief Creates a std::string representation of Balance object for logging,
     * \return string representation of Balance object
     */
    std::string toString() const;

private:

    // Called by process() to recompute mVolumes as needed.
    void setBalance(float balance);

    const float mCurveNorm;            // curve normalization, fixed constant.

                                       // process() sets cached mBalance through setBalance().
    float mBalance = 0.f;              // balance: -1.f (left), 0.f (center), 1.f (right)

    audio_channel_mask_t mChannelMask; // setChannelMask() sets the current channel mask
    size_t mChannelCount;              // derived from mChannelMask
    std::vector<int> mSides;           // per channel, the side (0 = left, 1 = right, 2 = center)

                                       // process() sets the cached mVolumes
    std::vector<float> mVolumes;       // per channel, the volume adjustment due to balance.
};

} // namespace android::audio_utils

#endif // !ANDROID_AUDIO_UTILS_BALANCE_H
