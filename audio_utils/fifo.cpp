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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// FIXME futex portion is not supported on macOS, should use the macOS alternative
#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#else
#define FUTEX_WAIT 0
#define FUTEX_WAIT_PRIVATE 0
#define FUTEX_WAKE 0
#define FUTEX_WAKE_PRIVATE 0
#endif

#include <audio_utils/fifo.h>
#include <audio_utils/roundup.h>
#include <cutils/log.h>
#include <utils/Errors.h>

#ifdef __linux__
#ifdef __ANDROID__
// bionic for Android provides clock_nanosleep
#else
// bionic for desktop Linux omits clock_nanosleep
int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request,
        struct timespec *remain)
{
    return syscall(SYS_clock_nanosleep, clock_id, flags, request, remain);
}
#endif  // __ANDROID__
#else   // __linux__
// macOS doesn't have clock_nanosleep
typedef int clockid_t;
#define CLOCK_MONOTONIC 0
int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request,
        struct timespec *remain)
{
    (void) clock_id;
    (void) flags;
    (void) request;
    (void) remain;
    errno = ENOSYS;
    return -1;
}
#endif  // __linux__

static int sys_futex(void *addr1, int op, int val1, const struct timespec *timeout, void *addr2,
        int val3)
{
#ifdef __linux__
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
#else   // __linux__
    // macOS doesn't have futex
    (void) addr1;
    (void) op;
    (void) val1;
    (void) timeout;
    (void) addr2;
    (void) val3;
    errno = ENOSYS;
    return -1;
#endif  // __linux__
}

audio_utils_fifo_base::audio_utils_fifo_base(uint32_t frameCount,
        audio_utils_fifo_index& writerRear, audio_utils_fifo_index *throttleFront)
        __attribute__((no_sanitize("integer"))) :
    mFrameCount(frameCount), mFrameCountP2(roundup(frameCount)),
    mFudgeFactor(mFrameCountP2 - mFrameCount),
    // FIXME need an API to configure the sync types
    mWriterRear(writerRear), mWriterRearSync(AUDIO_UTILS_FIFO_SYNC_SHARED),
    mThrottleFront(throttleFront), mThrottleFrontSync(AUDIO_UTILS_FIFO_SYNC_SHARED)
{
    // actual upper bound on frameCount will depend on the frame size
    LOG_ALWAYS_FATAL_IF(frameCount == 0 || frameCount > ((uint32_t) INT32_MAX));
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
        uint32_t rearOffset = rear & mask;
        uint32_t frontOffset = front & mask;
        if (rearOffset >= mFrameCount || frontOffset >= mFrameCount) {
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

audio_utils_fifo::audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer,
        audio_utils_fifo_index& writerRear, audio_utils_fifo_index *throttleFront)
        __attribute__((no_sanitize("integer"))) :
    audio_utils_fifo_base(frameCount, writerRear, throttleFront),
    mFrameSize(frameSize), mBuffer(buffer)
{
    // maximum value of frameCount * frameSize is INT32_MAX (2^31 - 1), not 2^31, because we need to
    // be able to distinguish successful and error return values from read and write.
    LOG_ALWAYS_FATAL_IF(frameCount == 0 || frameSize == 0 || buffer == NULL ||
            frameCount > ((uint32_t) INT32_MAX) / frameSize);
}

audio_utils_fifo::audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer,
        bool throttlesWriter) :
    audio_utils_fifo(frameCount, frameSize, buffer, mSingleProcessSharedRear,
        throttlesWriter ?  &mSingleProcessSharedFront : NULL)
{
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
    audio_utils_fifo_provider(), mFifo(fifo), mLocalRear(0),
    mLowLevelArm(fifo.mFrameCount), mHighLevelTrigger(0),
    mArmed(true),   // because initial fill level of zero is < mLowLevelArm
    mEffectiveFrames(fifo.mFrameCount)
{
}

audio_utils_fifo_writer::~audio_utils_fifo_writer()
{
}

ssize_t audio_utils_fifo_writer::write(const void *buffer, size_t count,
        const struct timespec *timeout)
        __attribute__((no_sanitize("integer")))
{
    audio_utils_iovec iovec[2];
    ssize_t availToWrite = obtain(iovec, count, timeout);
    if (availToWrite > 0) {
        memcpy((char *) mFifo.mBuffer + iovec[0].mOffset * mFifo.mFrameSize, buffer,
                iovec[0].mLength * mFifo.mFrameSize);
        if (iovec[1].mLength > 0) {
            memcpy((char *) mFifo.mBuffer + iovec[1].mOffset * mFifo.mFrameSize,
                    (char *) buffer + (iovec[0].mLength * mFifo.mFrameSize),
                    iovec[1].mLength * mFifo.mFrameSize);
        }
        release(availToWrite);
    }
    return availToWrite;
}

// iovec == NULL is not part of the public API, but internally it means don't set mObtained
ssize_t audio_utils_fifo_writer::obtain(audio_utils_iovec iovec[2], size_t count,
        const struct timespec *timeout)
        __attribute__((no_sanitize("integer")))
{
    int err = 0;
    size_t availToWrite;
    if (mFifo.mThrottleFront != NULL) {
        uint32_t front;
        for (;;) {
            front = atomic_load_explicit(&mFifo.mThrottleFront->mIndex, std::memory_order_acquire);
            int32_t filled = mFifo.diff(mLocalRear, front);
            if (filled < 0) {
                // on error, return an empty slice
                err = filled;
                availToWrite = 0;
                break;
            }
            availToWrite = mEffectiveFrames > (uint32_t) filled ?
                    mEffectiveFrames - (uint32_t) filled : 0;
            // TODO pull out "count == 0"
            if (count == 0 || availToWrite > 0 || timeout == NULL ||
                    (timeout->tv_sec == 0 && timeout->tv_nsec == 0)) {
                break;
            }
            // TODO add comments
            // TODO abstract out switch and replace by general sync object
            //      the high level code (synchronization, sleep, futex, iovec) should be completely
            //      separate from the low level code (indexes, available, masking).
            int op = FUTEX_WAIT;
            switch (mFifo.mThrottleFrontSync) {
            case AUDIO_UTILS_FIFO_SYNC_SLEEP:
                err = clock_nanosleep(CLOCK_MONOTONIC, 0 /*flags*/, timeout, NULL /*remain*/);
                if (err < 0) {
                    LOG_ALWAYS_FATAL_IF(errno != EINTR, "unexpected err=%d errno=%d", err, errno);
                    err = -errno;
                } else {
                    err = -ETIMEDOUT;
                }
                break;
            case AUDIO_UTILS_FIFO_SYNC_PRIVATE:
                op = FUTEX_WAIT_PRIVATE;
                // fall through
            case AUDIO_UTILS_FIFO_SYNC_SHARED:
                if (timeout->tv_sec == LONG_MAX) {
                    timeout = NULL;
                }
                err = sys_futex(&mFifo.mThrottleFront->mIndex, op, front, timeout, NULL, 0);
                if (err < 0) {
                    switch (errno) {
                    case EINTR:
                    case ETIMEDOUT:
                        err = -errno;
                        break;
                    default:
                        LOG_ALWAYS_FATAL("unexpected err=%d errno=%d", err, errno);
                        break;
                    }
                }
                break;
            default:
                LOG_ALWAYS_FATAL("mFifo.mThrottleFrontSync=%d", mFifo.mThrottleFrontSync);
                break;
            }
            timeout = NULL;
        }
    } else {
        availToWrite = mEffectiveFrames;
    }
    if (availToWrite > count) {
        availToWrite = count;
    }
    uint32_t rearOffset = mLocalRear & (mFifo.mFrameCountP2 - 1);
    size_t part1 = mFifo.mFrameCount - rearOffset;
    if (part1 > availToWrite) {
        part1 = availToWrite;
    }
    size_t part2 = part1 > 0 ? availToWrite - part1 : 0;
    // return slice
    if (iovec != NULL) {
        iovec[0].mOffset = rearOffset;
        iovec[0].mLength = part1;
        iovec[1].mOffset = 0;
        iovec[1].mLength = part2;
        mObtained = availToWrite;
    }
    return availToWrite > 0 ? availToWrite : err;
}

void audio_utils_fifo_writer::release(size_t count)
        __attribute__((no_sanitize("integer")))
{
    if (count > 0) {
        LOG_ALWAYS_FATAL_IF(count > mObtained);
        if (mFifo.mThrottleFront != NULL) {
            uint32_t front = atomic_load_explicit(&mFifo.mThrottleFront->mIndex,
                    std::memory_order_acquire);
            int32_t filled = mFifo.diff(mLocalRear, front);
            mLocalRear = mFifo.sum(mLocalRear, count);
            atomic_store_explicit(&mFifo.mWriterRear.mIndex, mLocalRear,
                    std::memory_order_release);
            // TODO add comments
            int op = FUTEX_WAKE;
            switch (mFifo.mWriterRearSync) {
            case AUDIO_UTILS_FIFO_SYNC_SLEEP:
                break;
            case AUDIO_UTILS_FIFO_SYNC_PRIVATE:
                op = FUTEX_WAKE_PRIVATE;
                // fall through
            case AUDIO_UTILS_FIFO_SYNC_SHARED:
                if (filled >= 0) {
                    if ((uint32_t) filled < mLowLevelArm) {
                        mArmed = true;
                    }
                    if (mArmed && filled + count > mHighLevelTrigger) {
                        int err = sys_futex(&mFifo.mWriterRear.mIndex,
                                op, INT32_MAX /*waiters*/, NULL, NULL, 0);
                        // err is number of processes woken up
                        if (err < 0) {
                            LOG_ALWAYS_FATAL("%s: unexpected err=%d errno=%d",
                                    __func__, err, errno);
                        }
                        mArmed = false;
                    }
                }
                break;
            default:
                LOG_ALWAYS_FATAL("mFifo.mWriterRearSync=%d", mFifo.mWriterRearSync);
                break;
            }
        } else {
            mLocalRear = mFifo.sum(mLocalRear, count);
            atomic_store_explicit(&mFifo.mWriterRear.mIndex, mLocalRear,
                    std::memory_order_release);
        }
        mObtained -= count;
    }
}

ssize_t audio_utils_fifo_writer::available()
{
    // iovec == NULL is not part of the public API, but internally it means don't set mObtained
    return obtain(NULL /*iovec*/, SIZE_MAX /*count*/, NULL /*timeout*/);
}

void audio_utils_fifo_writer::resize(uint32_t frameCount)
{
    // cap to range [0, mFifo.mFrameCount]
    if (frameCount > mFifo.mFrameCount) {
        frameCount = mFifo.mFrameCount;
    }
    // if we reduce the effective frame count, update hysteresis points to be within the new range
    if (frameCount < mEffectiveFrames) {
        if (mLowLevelArm > frameCount) {
            mLowLevelArm = frameCount;
        }
        if (mHighLevelTrigger > frameCount) {
            mHighLevelTrigger = frameCount;
        }
    }
    mEffectiveFrames = frameCount;
}

uint32_t audio_utils_fifo_writer::size() const
{
    return mEffectiveFrames;
}

void audio_utils_fifo_writer::setHysteresis(uint32_t lowLevelArm, uint32_t highLevelTrigger)
{
    // cap to range [0, mEffectiveFrames]
    if (lowLevelArm > mEffectiveFrames) {
        lowLevelArm = mEffectiveFrames;
    }
    if (highLevelTrigger > mEffectiveFrames) {
        highLevelTrigger = mEffectiveFrames;
    }
    // TODO this is overly conservative; it would be better to arm based on actual fill level
    if (lowLevelArm > mLowLevelArm) {
        mArmed = true;
    }
    mLowLevelArm = lowLevelArm;
    mHighLevelTrigger = highLevelTrigger;
}

void audio_utils_fifo_writer::getHysteresis(uint32_t *lowLevelArm, uint32_t *highLevelTrigger) const
{
    *lowLevelArm = mLowLevelArm;
    *highLevelTrigger = mHighLevelTrigger;
}

////////////////////////////////////////////////////////////////////////////////

audio_utils_fifo_reader::audio_utils_fifo_reader(audio_utils_fifo& fifo, bool throttlesWriter) :
    audio_utils_fifo_provider(), mFifo(fifo), mLocalFront(0),
    mThrottleFront(throttlesWriter ? mFifo.mThrottleFront : NULL),
    mHighLevelArm(-1), mLowLevelTrigger(mFifo.mFrameCount),
    mArmed(true)    // because initial fill level of zero is > mHighLevelArm
{
}

audio_utils_fifo_reader::~audio_utils_fifo_reader()
{
    // TODO Need a way to pass throttle capability to the another reader, should one reader exit.
}

ssize_t audio_utils_fifo_reader::read(void *buffer, size_t count, const struct timespec *timeout,
        size_t *lost)
        __attribute__((no_sanitize("integer")))
{
    audio_utils_iovec iovec[2];
    ssize_t availToRead = obtain(iovec, count, timeout, lost);
    if (availToRead > 0) {
        memcpy(buffer, (char *) mFifo.mBuffer + iovec[0].mOffset * mFifo.mFrameSize,
                iovec[0].mLength * mFifo.mFrameSize);
        if (iovec[1].mLength > 0) {
            memcpy((char *) buffer + (iovec[0].mLength * mFifo.mFrameSize),
                    (char *) mFifo.mBuffer + iovec[1].mOffset * mFifo.mFrameSize,
                    iovec[1].mLength * mFifo.mFrameSize);
        }
        release(availToRead);
    }
    return availToRead;
}

ssize_t audio_utils_fifo_reader::obtain(audio_utils_iovec iovec[2], size_t count,
        const struct timespec *timeout)
        __attribute__((no_sanitize("integer")))
{
    return obtain(iovec, count, timeout, NULL /*lost*/);
}

void audio_utils_fifo_reader::release(size_t count)
        __attribute__((no_sanitize("integer")))
{
    if (count > 0) {
        LOG_ALWAYS_FATAL_IF(count > mObtained);
        if (mThrottleFront != NULL) {
            uint32_t rear = atomic_load_explicit(&mFifo.mWriterRear.mIndex,
                    std::memory_order_acquire);
            int32_t filled = mFifo.diff(rear, mLocalFront);
            mLocalFront = mFifo.sum(mLocalFront, count);
            atomic_store_explicit(&mThrottleFront->mIndex, mLocalFront,
                    std::memory_order_release);
            // TODO add comments
            int op = FUTEX_WAKE;
            switch (mFifo.mThrottleFrontSync) {
            case AUDIO_UTILS_FIFO_SYNC_SLEEP:
                break;
            case AUDIO_UTILS_FIFO_SYNC_PRIVATE:
                op = FUTEX_WAKE_PRIVATE;
                // fall through
            case AUDIO_UTILS_FIFO_SYNC_SHARED:
                if (filled >= 0) {
                    if (filled > mHighLevelArm) {
                        mArmed = true;
                    }
                    if (mArmed && filled - count < mLowLevelTrigger) {
                        int err = sys_futex(&mThrottleFront->mIndex,
                                op, 1 /*waiters*/, NULL, NULL, 0);
                        // err is number of processes woken up
                        if (err < 0 || err > 1) {
                            LOG_ALWAYS_FATAL("%s: unexpected err=%d errno=%d",
                                    __func__, err, errno);
                        }
                        mArmed = false;
                    }
                }
                break;
            default:
                LOG_ALWAYS_FATAL("mFifo.mThrottleFrontSync=%d", mFifo.mThrottleFrontSync);
                break;
            }
        } else {
            mLocalFront = mFifo.sum(mLocalFront, count);
        }
        mObtained -= count;
    }
}

// iovec == NULL is not part of the public API, but internally it means don't set mObtained
ssize_t audio_utils_fifo_reader::obtain(audio_utils_iovec iovec[2], size_t count,
        const struct timespec *timeout, size_t *lost)
        __attribute__((no_sanitize("integer")))
{
    int err = 0;
    uint32_t rear;
    for (;;) {
        rear = atomic_load_explicit(&mFifo.mWriterRear.mIndex,
                std::memory_order_acquire);
        // TODO pull out "count == 0"
        if (count == 0 || rear != mLocalFront || timeout == NULL ||
                (timeout->tv_sec == 0 && timeout->tv_nsec == 0)) {
            break;
        }
        // TODO add comments
        int op = FUTEX_WAIT;
        switch (mFifo.mWriterRearSync) {
        case AUDIO_UTILS_FIFO_SYNC_SLEEP:
            err = clock_nanosleep(CLOCK_MONOTONIC, 0 /*flags*/, timeout, NULL /*remain*/);
            if (err < 0) {
                LOG_ALWAYS_FATAL_IF(errno != EINTR, "unexpected err=%d errno=%d", err, errno);
                err = -errno;
            } else {
                err = -ETIMEDOUT;
            }
            break;
        case AUDIO_UTILS_FIFO_SYNC_PRIVATE:
            op = FUTEX_WAIT_PRIVATE;
            // fall through
        case AUDIO_UTILS_FIFO_SYNC_SHARED:
            if (timeout->tv_sec == LONG_MAX) {
                timeout = NULL;
            }
            err = sys_futex(&mFifo.mWriterRear.mIndex, op, rear, timeout, NULL, 0);
            if (err < 0) {
                switch (errno) {
                case EINTR:
                case ETIMEDOUT:
                    err = -errno;
                    break;
                default:
                    LOG_ALWAYS_FATAL("unexpected err=%d errno=%d", err, errno);
                    break;
                }
            }
            break;
        default:
            LOG_ALWAYS_FATAL("mFifo.mWriterRearSync=%d", mFifo.mWriterRearSync);
            break;
        }
        timeout = NULL;
    }
    int32_t filled = mFifo.diff(rear, mLocalFront, lost);
    if (filled < 0) {
        if (filled == -EOVERFLOW) {
            mLocalFront = rear;
        }
        // on error, return an empty slice
        err = filled;
        filled = 0;
    }
    size_t availToRead = (size_t) filled;
    if (availToRead > count) {
        availToRead = count;
    }
    uint32_t frontOffset = mLocalFront & (mFifo.mFrameCountP2 - 1);
    size_t part1 = mFifo.mFrameCount - frontOffset;
    if (part1 > availToRead) {
        part1 = availToRead;
    }
    size_t part2 = part1 > 0 ? availToRead - part1 : 0;
    // return slice
    if (iovec != NULL) {
        iovec[0].mOffset = frontOffset;
        iovec[0].mLength = part1;
        iovec[1].mOffset = 0;
        iovec[1].mLength = part2;
        mObtained = availToRead;
    }
    return availToRead > 0 ? availToRead : err;
}

ssize_t audio_utils_fifo_reader::available()
{
    return available(NULL /*lost*/);
}

ssize_t audio_utils_fifo_reader::available(size_t *lost)
{
    // iovec == NULL is not part of the public API, but internally it means don't set mObtained
    return obtain(NULL /*iovec*/, SIZE_MAX /*count*/, NULL /*timeout*/, lost);
}

void audio_utils_fifo_reader::setHysteresis(int32_t highLevelArm, uint32_t lowLevelTrigger)
{
    // cap to range [0, mFifo.mFrameCount]
    if (highLevelArm < 0) {
        highLevelArm = -1;
    } else if ((uint32_t) highLevelArm > mFifo.mFrameCount) {
        highLevelArm = mFifo.mFrameCount;
    }
    if (lowLevelTrigger > mFifo.mFrameCount) {
        lowLevelTrigger = mFifo.mFrameCount;
    }
    // TODO this is overly conservative; it would be better to arm based on actual fill level
    if (highLevelArm < mHighLevelArm) {
        mArmed = true;
    }
    mHighLevelArm = highLevelArm;
    mLowLevelTrigger = lowLevelTrigger;
}

void audio_utils_fifo_reader::getHysteresis(int32_t *highLevelArm, uint32_t *lowLevelTrigger) const
{
    *highLevelArm = mHighLevelArm;
    *lowLevelTrigger = mLowLevelTrigger;
}
