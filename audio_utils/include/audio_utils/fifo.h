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

#ifndef ANDROID_AUDIO_FIFO_H
#define ANDROID_AUDIO_FIFO_H

#include <atomic>
#include <stdlib.h>

#ifndef __cplusplus
#error C API is no longer supported
#endif

// Single writer, single reader non-blocking FIFO.
// Writer and reader must be in same process.

// No user-serviceable parts within.
class audio_utils_fifo {

public:

/**
 * Initialize a FIFO object.
 *
 *  \param frameCount  Max number of significant frames to be stored in the FIFO > 0.
 *                     If writes and reads always use the same count, and that count is a divisor of
 *                     frameCount, then the writes and reads will never do a partial transfer.
 *  \param frameSize   Size of each frame in bytes > 0, and frameSize * frameCount <= INT_MAX.
 *  \param buffer      Pointer to a caller-allocated buffer of frameCount frames.
 */
    audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer);

/**
 * De-initialize a FIFO object.
 */
    ~audio_utils_fifo();

private:

    // These fields are const after initialization
    const uint32_t mFrameCount;   // max number of significant frames to be stored in the FIFO > 0
    const uint32_t mFrameCountP2; // roundup(mFrameCount)
    const uint32_t mFudgeFactor;  // mFrameCountP2 - mFrameCount, the number of "wasted" frames
                                  // after the end of mBuffer.  Only the indices are wasted, not any
                                  // memory.
    const uint32_t mFrameSize;    // size of each frame in bytes
    void * const mBuffer;         // pointer to caller-allocated buffer of size mFrameCount frames

    // These are accessed by one side only using ordinary operations
    uint32_t     mLocalFront;   // frame index of first frame slot available to read, or read index
    uint32_t     mLocalRear;    // frame index of next frame slot available to write, or write index

    // These are accessed by both sides using atomic operations
    std::atomic_uint_fast32_t mSharedFront;
    std::atomic_uint_fast32_t mSharedRear;

public:

/**
 * Write to FIFO.
 *
 * \param buffer    Pointer to source buffer containing 'count' frames of data.
 * \param count     Desired number of frames to write.
 *
 * \return actual number of frames written <= count.
 *
 * The actual transfer count may be zero if the FIFO is full,
 * or partial if the FIFO was almost full.
 * A negative return value indicates an error.  Currently there are no errors defined.
 */
    ssize_t write(const void *buffer, size_t count);

/** Read from FIFO.
 *
 * \param buffer    Pointer to destination buffer to be filled with up to 'count' frames of data.
 * \param count     Desired number of frames to read.
 *
 * \return actual number of frames read <= count.
 *
 * The actual transfer count may be zero if the FIFO is empty,
 * or partial if the FIFO was almost empty.
 * A negative return value indicates an error.  Currently there are no errors defined.
 */
    ssize_t read(void *buffer, size_t count);

private:

/** Return a new index as the sum of a validated index and a specified increment.
 *
 * \param index     Caller should supply a validated mFront or mRear.
 * \param increment Value to be added to the index <= mFrameCount.
 *
 * \return the sum of index plus increment.
 */
    uint32_t sum(uint32_t index, uint32_t increment);

/** Return the difference between two indices: rear - front.
 *
 * \param rear     Caller should supply an unvalidated mRear.
 * \param front    Caller should supply an unvalidated mFront.
 *
 * \return the zero or positive difference <= mFrameCount, or a negative error code.
 */
    int32_t diff(uint32_t rear, uint32_t front);

};

#endif  // !ANDROID_AUDIO_FIFO_H
