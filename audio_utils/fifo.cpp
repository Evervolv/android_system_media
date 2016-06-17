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

audio_utils_fifo_base::audio_utils_fifo_base(uint32_t frameCount)
        __attribute__((no_sanitize("integer"))) :
    mFrameCount(frameCount), mFrameCountP2(roundup(frameCount)),
    mFudgeFactor(mFrameCountP2 - mFrameCount),
    mSharedRear(0), mThrottleFront(NULL)
{
    // actual upper bound on frameCount will depend on the frame size
    ALOG_ASSERT(frameCount > 0 && frameCount <= ((uint32_t) INT_MAX));
}

audio_utils_fifo_base::~audio_utils_fifo_base()
{
}

uint32_t audio_utils_fifo_base::sum(uint32_t index, uint32_t increment)
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

int32_t audio_utils_fifo_base::diff(uint32_t rear, uint32_t front, size_t *lost)
        __attribute__((no_sanitize("integer")))
{
    uint32_t diff = rear - front;
    if (mFudgeFactor) {
        uint32_t mask = mFrameCountP2 - 1;
        uint32_t rearMasked = rear & mask;
        uint32_t frontMasked = front & mask;
        if (rearMasked >= mFrameCount || frontMasked >= mFrameCount) {
            return -EIO;
        }
        uint32_t genDiff = (rear & ~mask) - (front & ~mask);
        if (genDiff != 0) {
            if (genDiff > mFrameCountP2) {
                if (lost != NULL) {
                    // TODO provide a more accurate estimate
                    *lost = (genDiff / mFrameCountP2) * mFrameCount;
                }
                return -EOVERFLOW;
            }
            diff -= mFudgeFactor;
        }
    }
    // FIFO should not be overfull
    if (diff > mFrameCount) {
        if (lost != NULL) {
            *lost = diff - mFrameCount;
        }
        return -EOVERFLOW;
    }
    return (int32_t) diff;
}

////////////////////////////////////////////////////////////////////////////////

audio_utils_fifo::audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer)
        __attribute__((no_sanitize("integer"))) :
    audio_utils_fifo_base(frameCount), mFrameSize(frameSize), mBuffer(buffer)
{
    // maximum value of frameCount * frameSize is INT_MAX (2^31 - 1), not 2^31, because we need to
    // be able to distinguish successful and error return values from read and write.
    ALOG_ASSERT(frameCount > 0 && frameSize > 0 && buffer != NULL &&
            frameCount <= ((uint32_t) INT_MAX) / frameSize);
}

audio_utils_fifo::~audio_utils_fifo()
{
}

////////////////////////////////////////////////////////////////////////////////

audio_utils_fifo_provider::audio_utils_fifo_provider() :
    mObtained(0)
{
}

audio_utils_fifo_provider::~audio_utils_fifo_provider()
{
}

////////////////////////////////////////////////////////////////////////////////

audio_utils_fifo_writer::audio_utils_fifo_writer(audio_utils_fifo& fifo) :
    audio_utils_fifo_provider(), mFifo(fifo), mLocalRear(0)
{
}

audio_utils_fifo_writer::~audio_utils_fifo_writer()
{
}

ssize_t audio_utils_fifo_writer::write(const void *buffer, size_t count)
        __attribute__((no_sanitize("integer")))
{
    audio_utils_iovec iovec[2];
    ssize_t availToWrite = obtain(iovec, count);
    if (availToWrite > 0) {
        memcpy(iovec[0].mBase, buffer, iovec[0].mLen * mFifo.mFrameSize);
        if (iovec[1].mLen > 0) {
            memcpy(iovec[1].mBase, (char *) buffer + (iovec[0].mLen * mFifo.mFrameSize),
                    iovec[1].mLen * mFifo.mFrameSize);
        }
        release(availToWrite);
    }
    return availToWrite;
}

ssize_t audio_utils_fifo_writer::obtain(audio_utils_iovec iovec[2], size_t count)
        __attribute__((no_sanitize("integer")))
{
    size_t availToWrite;
    if (mFifo.mThrottleFront != NULL) {
        uint32_t front = (uint32_t) atomic_load_explicit(mFifo.mThrottleFront,
                std::memory_order_acquire);
        int32_t filled = mFifo.diff(mLocalRear, front, NULL /*lost*/);
        if (filled < 0) {
            mObtained = 0;
            return (ssize_t) filled;
        }
        availToWrite = (size_t) mFifo.mFrameCount - (size_t) filled;
    } else {
        availToWrite = mFifo.mFrameCount;
    }
    if (availToWrite > count) {
        availToWrite = count;
    }
    uint32_t rearMasked = mLocalRear & (mFifo.mFrameCountP2 - 1);
    size_t part1 = mFifo.mFrameCount - rearMasked;
    if (part1 > availToWrite) {
        part1 = availToWrite;
    }
    size_t part2 = part1 > 0 ? availToWrite - part1 : 0;
    iovec[0].mLen = part1;
    iovec[0].mBase = part1 > 0 ? (char *) mFifo.mBuffer + (rearMasked * mFifo.mFrameSize) : NULL;
    iovec[1].mLen = part2;
    iovec[1].mBase = part2 > 0 ? mFifo.mBuffer : NULL;
    mObtained = availToWrite;
    return availToWrite;
}

void audio_utils_fifo_writer::release(size_t count)
        __attribute__((no_sanitize("integer")))
{
    if (count > 0) {
        ALOG_ASSERT(count <= mObtained);
        mLocalRear = mFifo.sum(mLocalRear, count);
        atomic_store_explicit(&mFifo.mSharedRear, (uint_fast32_t) mLocalRear,
                std::memory_order_release);
        mObtained -= count;
    }
}

////////////////////////////////////////////////////////////////////////////////

audio_utils_fifo_reader::audio_utils_fifo_reader(audio_utils_fifo& fifo, bool throttlesWriter) :
    audio_utils_fifo_provider(), mFifo(fifo), mLocalFront(0), mSharedFront(0)
{
    if (throttlesWriter) {
        fifo.mThrottleFront = &mSharedFront;
    }
}

audio_utils_fifo_reader::~audio_utils_fifo_reader()
{
}

ssize_t audio_utils_fifo_reader::read(void *buffer, size_t count, size_t *lost)
        __attribute__((no_sanitize("integer")))
{
    audio_utils_iovec iovec[2];
    ssize_t availToRead = obtain(iovec, count, lost);
    if (availToRead > 0) {
        memcpy(buffer, iovec[0].mBase, iovec[0].mLen * mFifo.mFrameSize);
        if (iovec[1].mLen > 0) {
            memcpy((char *) buffer + (iovec[0].mLen * mFifo.mFrameSize), iovec[1].mBase,
                    iovec[1].mLen * mFifo.mFrameSize);
        }
        release(availToRead);
    }
    return availToRead;
}

ssize_t audio_utils_fifo_reader::obtain(audio_utils_iovec iovec[2], size_t count)
        __attribute__((no_sanitize("integer")))
{
    return obtain(iovec, count, NULL);
}

void audio_utils_fifo_reader::release(size_t count)
        __attribute__((no_sanitize("integer")))
{
    if (count > 0) {
        ALOG_ASSERT(count <= mObtained);
        mLocalFront = mFifo.sum(mLocalFront, count);
        if (mFifo.mThrottleFront == &mSharedFront) {
            atomic_store_explicit(&mSharedFront, (uint_fast32_t) mLocalFront,
                    std::memory_order_release);
        }
        mObtained -= count;
    }
}

ssize_t audio_utils_fifo_reader::obtain(audio_utils_iovec iovec[2], size_t count, size_t *lost)
        __attribute__((no_sanitize("integer")))
{
    uint32_t rear = (uint32_t) atomic_load_explicit(&mFifo.mSharedRear,
            std::memory_order_acquire);
    int32_t filled = mFifo.diff(rear, mLocalFront, lost);
    if (filled < 0) {
        if (filled == android::BAD_INDEX) {
            mLocalFront = rear;
        }
        mObtained = 0;
        return (ssize_t) filled;
    }
    size_t availToRead = (size_t) filled;
    if (availToRead > count) {
        availToRead = count;
    }
    uint32_t frontMasked = mLocalFront & (mFifo.mFrameCountP2 - 1);
    size_t part1 = mFifo.mFrameCount - frontMasked;
    if (part1 > availToRead) {
        part1 = availToRead;
    }
    size_t part2 = part1 > 0 ? availToRead - part1 : 0;
    iovec[0].mLen = part1;
    iovec[0].mBase = part1 > 0 ? (char *) mFifo.mBuffer + (frontMasked * mFifo.mFrameSize) : NULL;
    iovec[1].mLen = part2;
    iovec[1].mBase = part2 > 0 ? mFifo.mBuffer : NULL;
    mObtained = availToRead;
    return availToRead;
}
