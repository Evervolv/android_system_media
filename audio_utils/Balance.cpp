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

#include <audio_utils/Balance.h>

namespace android::audio_utils {

// Implementation detail: parametric volume curve transfer function

constexpr float CURVE_PARAMETER = 2.f;

static inline float curve(float parameter, float inVolume) {
    return exp(parameter * inVolume) - 1.f;
}

Balance::Balance()
    : mCurveNorm(1.f / curve(CURVE_PARAMETER, 1.f /* inVolume */))
{
    setChannelMask(AUDIO_CHANNEL_OUT_STEREO);
}

void Balance::setChannelMask(audio_channel_mask_t channelMask)
{
    channelMask &= ~ AUDIO_CHANNEL_HAPTIC_ALL;
    if (!audio_is_output_channel(channelMask) // invalid mask
            || mChannelMask == channelMask) { // no need to do anything
        return;
    }

    mChannelMask = channelMask;
    mChannelCount = audio_channel_count_from_out_mask(channelMask);

    // reset mVolumes (the next process() will recalculate if needed).
    mVolumes.resize(mChannelCount);
    std::fill(mVolumes.begin(), mVolumes.end(), 1.f);
    mBalance = 0.f;

    // Implementation detail (may change):
    // For implementation speed, we precompute the side (left, right, center),
    // which is a fixed geometrical constant for a given channel mask.
    // This assumes that the channel mask does not change frequently.
    //
    // For the channel mask spec, see system/media/audio/include/system/audio-base.h.
    //
    // The side is: 0 = left, 1 = right, 2 = center.
    static constexpr int sideFromChannel[] = {
        0, // AUDIO_CHANNEL_OUT_FRONT_LEFT            = 0x1u,
        1, // AUDIO_CHANNEL_OUT_FRONT_RIGHT           = 0x2u,
        2, // AUDIO_CHANNEL_OUT_FRONT_CENTER          = 0x4u,
        2, // AUDIO_CHANNEL_OUT_LOW_FREQUENCY         = 0x8u,
        0, // AUDIO_CHANNEL_OUT_BACK_LEFT             = 0x10u,
        1, // AUDIO_CHANNEL_OUT_BACK_RIGHT            = 0x20u,
        0, // AUDIO_CHANNEL_OUT_FRONT_LEFT_OF_CENTER  = 0x40u,
        1, // AUDIO_CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x80u,
        2, // AUDIO_CHANNEL_OUT_BACK_CENTER           = 0x100u,
        0, // AUDIO_CHANNEL_OUT_SIDE_LEFT             = 0x200u,
        1, // AUDIO_CHANNEL_OUT_SIDE_RIGHT            = 0x400u,
        2, // AUDIO_CHANNEL_OUT_TOP_CENTER            = 0x800u,
        0, // AUDIO_CHANNEL_OUT_TOP_FRONT_LEFT        = 0x1000u,
        2, // AUDIO_CHANNEL_OUT_TOP_FRONT_CENTER      = 0x2000u,
        1, // AUDIO_CHANNEL_OUT_TOP_FRONT_RIGHT       = 0x4000u,
        0, // AUDIO_CHANNEL_OUT_TOP_BACK_LEFT         = 0x8000u,
        2, // AUDIO_CHANNEL_OUT_TOP_BACK_CENTER       = 0x10000u,
        1, // AUDIO_CHANNEL_OUT_TOP_BACK_RIGHT        = 0x20000u,
        0, // AUDIO_CHANNEL_OUT_TOP_SIDE_LEFT         = 0x40000u,
        1, // AUDIO_CHANNEL_OUT_TOP_SIDE_RIGHT        = 0x80000u,
     };

    mSides.resize(mChannelCount);
    for (unsigned i = 0, channel = channelMask; channel != 0; ++i) {
        const int index = __builtin_ctz(channel);
        if (index < sizeof(sideFromChannel) / sizeof(sideFromChannel[0])) {
            mSides[i] = 2; // consider center
        } else {
            mSides[i] = sideFromChannel[index];
        }
        channel &= ~(1 << index);
    }
}

void Balance::process(float *buffer, float balance, size_t frames)
{
    setBalance(balance);
    for (size_t i = 0; i < frames; ++i) {
        for (size_t j = 0; j < mChannelCount; ++j) {
            *buffer++ *= mVolumes[j];
        }
    }
}

// Implementation detail (may change):
// This is not an energy preserving balance (e.g. using sin/cos cross fade or some such).
// Rather it preserves full gain on left and right when balance is 0.f,
// and decreases the right or left as one changes the balance.
void Balance::computeStereoBalance(float balance, float *left, float *right) const
{
    if (balance > 0.f) {
        *left = curve(CURVE_PARAMETER, 1.f - balance) * mCurveNorm;
        *right = 1.f;
    } else if (balance < 0.f) {
        *left = 1.f;
        *right = curve(CURVE_PARAMETER, 1.f + balance) * mCurveNorm;
    } else {
        *left = 1.f;
        *right = 1.f;
    }

    // Functionally:
    // *left = balance > 0.f ? curve(CURVE_PARAMETER, 1.f - balance) * mCurveNorm : 1.f;
    // *right = balance < 0.f ? curve(CURVE_PARAMETER, 1.f + balance) * mCurveNorm : 1.f;
}

std::string Balance::toString() const
{
    std::stringstream ss;
    ss << "balance " << mBalance << " channelCount " << mChannelCount << " volumes:";
    for (float volume : mVolumes) {
        ss << " " << volume;
    }
    return ss.str();
}

void Balance::setBalance(float balance)
{
    if (isnan(balance) || fabs(balance) > 1.f              // balance out of range
            || mBalance == balance || mChannelCount < 2) { // change not applicable
        return;
    }

    mBalance = balance;

    // handle the common cases
    if (mChannelMask == AUDIO_CHANNEL_OUT_STEREO
            || audio_channel_mask_get_representation(mChannelMask)
                    == AUDIO_CHANNEL_REPRESENTATION_INDEX) {
        computeStereoBalance(balance, &mVolumes[0], &mVolumes[1]);
        return;
    }

    float balanceVolumes[3]; // left, right, center
    computeStereoBalance(balance, &balanceVolumes[0], &balanceVolumes[1]);
    balanceVolumes[2] = 1.f; // center  TODO: consider center scaling.

    for (size_t i = 0; i < mVolumes.size(); ++i) {
        mVolumes[i] = balanceVolumes[mSides[i]];
    }
}

} // namespace android::audio_utils

