/*
 * Copyright (C) 2015 The Android Open Source Project
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
#define LOG_TAG "audio_utils_fifo"

#include <stdlib.h>
#include <string.h>
#include <audio_utils/fifo.h>
#include <audio_utils/roundup.h>
#include <cutils/log.h>
#include <utils/Errors.h>

audio_utils_fifo::audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer) :
    mFrameCount(frameCount), mFrameCountP2(roundup(frameCount)),
    mFudgeFactor(mFrameCountP2 - mFrameCount), mFrameSize(frameSize), mBuffer(buffer),
    mLocalFront(0), mLocalRear(0), mSharedFront(0), mSharedRear(0)
{
    // maximum value of frameCount * frameSize is INT_MAX (2^31 - 1), not 2^31, because we need to
    // be able to distinguish successful and error return values from read and write.
    ALOG_ASSERT(frameCount > 0 && frameSize > 0 && buffer != NULL &&
            frameCount <= ((uint32_t) INT_MAX) / frameSize);
}

audio_utils_fifo::~audio_utils_fifo()
{
}

uint32_t audio_utils_fifo::sum(uint32_t index, uint32_t increment)
        __attribute__((no_sanitize("integer")))
{
    if (mFudgeFactor) {
        uint32_t mask = mFrameCountP2 - 1;
        ALOG_ASSERT((index & mask) < mFrameCount);
        ALOG_ASSERT(increment <= mFrameCountP2);
        if ((index & mask) + increment >= mFrameCount) {
            increment += mFudgeFactor;
        }
        index += increment;
        ALOG_ASSERT((index & mask) < mFrameCount);
        return index;
    } else {
        return index + increment;
    }
}

int32_t audio_utils_fifo::diff(uint32_t rear, uint32_t front)
        __attribute__((no_sanitize("integer")))
{
    uint32_t diff = rear - front;
    if (mFudgeFactor) {
        uint32_t mask = mFrameCountP2 - 1;
        uint32_t rearMasked = rear & mask;
        uint32_t frontMasked = front & mask;
        if (rearMasked >= mFrameCount || frontMasked >= mFrameCount) {
            return (int32_t) android::UNKNOWN_ERROR;
        }
        uint32_t genDiff = (rear & ~mask) - (front & ~mask);
        if (genDiff != 0) {
            if (genDiff > mFrameCountP2) {
                return (int32_t) android::UNKNOWN_ERROR;
            }
            diff -= mFudgeFactor;
        }
    }
    // FIFO should not be overfull
    if (diff > mFrameCount) {
        return (int32_t) android::UNKNOWN_ERROR;
    }
    return (int32_t) diff;
}

ssize_t audio_utils_fifo::write(const void *buffer, size_t count)
        __attribute__((no_sanitize("integer")))
{
    uint32_t front = (uint32_t) atomic_load_explicit(&mSharedFront, std::memory_order_acquire);
    uint32_t rear = mLocalRear;
    int32_t filled = diff(rear, front);
    if (filled < 0) {
        return (ssize_t) filled;
    }
    size_t availToWrite = (size_t) mFrameCount - (size_t) filled;
    if (availToWrite > count) {
        availToWrite = count;
    }
    uint32_t rearMasked = rear & (mFrameCountP2 - 1);
    size_t part1 = mFrameCount - rearMasked;
    if (part1 > availToWrite) {
        part1 = availToWrite;
    }
    if (part1 > 0) {
        memcpy((char *) mBuffer + (rearMasked * mFrameSize), buffer, part1 * mFrameSize);
        size_t part2 = availToWrite - part1;
        if (part2 > 0) {
            memcpy(mBuffer, (char *) buffer + (part1 * mFrameSize), part2 * mFrameSize);
        }
        mLocalRear = sum(rear, availToWrite);
        atomic_store_explicit(&mSharedRear, (uint_fast32_t) mLocalRear,
                std::memory_order_release);
    }
    return availToWrite;
}

ssize_t audio_utils_fifo::read(void *buffer, size_t count)
        __attribute__((no_sanitize("integer")))
{
    uint32_t rear = (uint32_t) atomic_load_explicit(&mSharedRear, std::memory_order_acquire);
    uint32_t front = mLocalFront;
    int32_t filled = diff(rear, front);
    if (filled < 0) {
        return (ssize_t) filled;
    }
    size_t availToRead = (size_t) filled;
    if (availToRead > count) {
        availToRead = count;
    }
    uint32_t frontMasked = front & (mFrameCountP2 - 1);
    size_t part1 = mFrameCount - frontMasked;
    if (part1 > availToRead) {
        part1 = availToRead;
    }
    if (part1 > 0) {
        memcpy(buffer, (char *) mBuffer + (frontMasked * mFrameSize), part1 * mFrameSize);
        size_t part2 = availToRead - part1;
        if (part2 > 0) {
            memcpy((char *) buffer + (part1 * mFrameSize), mBuffer, part2 * mFrameSize);
        }
        mLocalFront = sum(front, availToRead);
        atomic_store_explicit(&mSharedFront, (uint_fast32_t) mLocalFront,
                std::memory_order_release);
    }
    return availToRead;
}
