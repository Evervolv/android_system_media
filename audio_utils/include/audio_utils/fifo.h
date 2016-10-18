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

/**
 * An index that may optionally be placed in shared memory.
 * Must be Plain Old Data (POD), so no virtual methods are allowed.
 * If in shared memory, exactly one process must explicitly call the constructor via placement new.
 * \see #audio_utils_fifo_sync
 */
struct audio_utils_fifo_index {
    friend class audio_utils_fifo_reader;
    friend class audio_utils_fifo_writer;

public:
    audio_utils_fifo_index() : mIndex(0) { }
    ~audio_utils_fifo_index() { }

private:
    // Linux futex is 32 bits regardless of platform.
    // It would make more sense to declare this as atomic_uint32_t, but there is no such type name.
    std::atomic_uint_least32_t  mIndex; // accessed by both sides using atomic operations
    static_assert(sizeof(mIndex) == sizeof(uint32_t), "mIndex must be 32 bits");

    // TODO Abstract out atomic operations to here
    // TODO Replace friend by setter and getter, and abstract the futex
};

/** Indicates whether an index is also used for synchronization. */
enum audio_utils_fifo_sync {
    /** Index is not also used for synchronization; timeouts are done via clock_nanosleep(). */
    AUDIO_UTILS_FIFO_SYNC_SLEEP,
    /** Index is also used for synchronization as futex, and is mapped by one process. */
    AUDIO_UTILS_FIFO_SYNC_PRIVATE,
    /** Index is also used for synchronization as futex, and is mapped by one or more processes. */
    AUDIO_UTILS_FIFO_SYNC_SHARED,
};

/**
 * Base class for single-writer, single-reader or multi-reader, optionally blocking FIFO.
 * The base class manipulates frame indices only, and has no knowledge of frame sizes or the buffer.
 * At most one reader, called the "throttling reader", can block the writer.
 * The "fill level", or unread frame count, is defined with respect to the throttling reader.
 */
class audio_utils_fifo_base {

protected:

    /**
     * Construct FIFO base class
     *
     *  \param frameCount    Maximum usable frames to be stored in the FIFO > 0 && <= INT32_MAX,
     *                       aka "capacity".
     *                       If release()s always use the same count, and the count is a divisor of
     *                       (effective) \p frameCount, then the obtain()s won't ever be fragmented.
     *  \param writerRear    Writer's rear index.  Passed by reference because it must be non-NULL.
     *  \param throttleFront Pointer to the front index of at most one reader that throttles the
     *                       writer, or NULL for no throttling.
     */
    audio_utils_fifo_base(uint32_t frameCount, audio_utils_fifo_index& writerRear,
            audio_utils_fifo_index *throttleFront = NULL);
    /*virtual*/ ~audio_utils_fifo_base();

    /** Return a new index as the sum of a validated index and a specified increment.
     *
     * \param index     Caller should supply a validated mFront or mRear.
     * \param increment Value to be added to the index <= mFrameCount.
     *
     * \return The sum of index plus increment.
     */
    uint32_t sum(uint32_t index, uint32_t increment);

    /** Return the difference between two indices: rear - front.
     *
     * \param rear     Caller should supply an unvalidated mRear.
     * \param front    Caller should supply an unvalidated mFront.
     * \param lost     If non-NULL, set to the approximate number of lost frames.
     *
     * \return The zero or positive difference <= mFrameCount, or a negative error code.
     */
    int32_t diff(uint32_t rear, uint32_t front, size_t *lost = NULL);

    // These fields are const after initialization

    /** Maximum usable frames to be stored in the FIFO > 0 && <= INT32_MAX, aka "capacity" */
    const uint32_t mFrameCount;
    /** Equal to roundup(mFrameCount) */
    const uint32_t mFrameCountP2;

    /**
     * Equal to mFrameCountP2 - mFrameCount, the number of "wasted" frames after the end of mBuffer.
     * Only the indices are wasted, not any memory.
     */
    const uint32_t mFudgeFactor;

    /** Reference to writer's rear index. */
    audio_utils_fifo_index&     mWriterRear;
    /** Indicates how synchronization is done for mWriterRear. */
    audio_utils_fifo_sync       mWriterRearSync;

    /**
     * Pointer to the front index of at most one reader that throttles the writer,
     * or NULL for no throttling.
     */
    audio_utils_fifo_index*     mThrottleFront;
    /** Indicates how synchronization is done for mThrottleFront. */
    audio_utils_fifo_sync       mThrottleFrontSync;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Same as audio_utils_fifo_base, but understands frame sizes and knows about the buffer but does
 * not own it.
 */
class audio_utils_fifo : audio_utils_fifo_base {

    friend class audio_utils_fifo_reader;
    friend class audio_utils_fifo_writer;

public:

    /**
     * Construct a FIFO object: multi-process.
     *
     *  \param frameCount  Maximum usable frames to be stored in the FIFO > 0 && <= INT32_MAX,
     *                     aka "capacity".
     *                     If writes and reads always use the same count, and the count is a divisor
     *                     of \p frameCount, then the writes and reads won't do a partial transfer.
     *  \param frameSize   Size of each frame in bytes > 0,
     *                     \p frameSize * \p frameCount <= INT32_MAX.
     *  \param buffer      Pointer to a caller-allocated buffer of \p frameCount frames.
     *  \param writerRear  Writer's rear index.  Passed by reference because it must be non-NULL.
     *  \param throttleFront Pointer to the front index of at most one reader that throttles the
     *                       writer, or NULL for no throttling.
     */
    audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer,
            audio_utils_fifo_index& writerRear, audio_utils_fifo_index *throttleFront = NULL);

    /**
     * Construct a FIFO object: single-process.
     *  \param frameCount  Maximum usable frames to be stored in the FIFO > 0 && <= INT32_MAX,
     *                     aka "capacity".
     *                     If writes and reads always use the same count, and the count is a divisor
     *                     of \p frameCount, then the writes and reads won't do a partial transfer.
     *  \param frameSize   Size of each frame in bytes > 0,
     *                     \p frameSize * \p frameCount <= INT32_MAX.
     *  \param buffer      Pointer to a caller-allocated buffer of \p frameCount frames.
     *  \param throttlesWriter Whether there is one reader that throttles the writer.
     */
    audio_utils_fifo(uint32_t frameCount, uint32_t frameSize, void *buffer,
            bool throttlesWriter = true);

    /*virtual*/ ~audio_utils_fifo();

private:

    // These fields are const after initialization
    const uint32_t mFrameSize;  // size of each frame in bytes
    void * const   mBuffer;     // pointer to caller-allocated buffer of size mFrameCount frames

    // only used for single-process constructor
    audio_utils_fifo_index      mSingleProcessSharedRear;

    // only used for single-process constructor when throttlesWriter == true
    audio_utils_fifo_index      mSingleProcessSharedFront;
};

/**
 * Describes one virtually contiguous fragment of a logically contiguous slice.
 * Compare to struct iovec for readv(2) and writev(2).
 */
struct audio_utils_iovec {
    /** Offset of fragment in frames, relative to mBuffer, undefined if mLength == 0 */
    uint32_t    mOffset;
    /** Length of fragment in frames, 0 means fragment is empty */
    uint32_t    mLength;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Based on frameworks/av/include/media/AudioBufferProvider.h
 */
class audio_utils_fifo_provider {
public:
    audio_utils_fifo_provider();
    virtual ~audio_utils_fifo_provider();

    /**
     * Obtain access to a logically contiguous slice of a stream, represented by \p iovec.
     * For the reader(s), the slice is initialized and has read-only access.
     * For the writer, the slice is uninitialized and has read/write access.
     * It is permitted to call obtain() multiple times without an intervening release().
     * Each call resets the notion of most recently obtained slice.
     *
     * \param iovec Non-NULL pointer to a pair of fragment descriptors.
     *              On entry, the descriptors may be uninitialized.
     *              On exit, the descriptors are initialized and refer to each of the two fragments.
     *              iovec[0] describes the initial fragment of the slice, and
     *              iovec[1] describes the remaining non-virtually-contiguous fragment.
     *              Empty iovec[0] implies that iovec[1] is also empty.
     * \param count The maximum number of frames to obtain.
     *              See the high/low setpoints for something which is close to, but not the same as,
     *              a minimum.
     * \param timeout Indicates the maximum time to block for at least one frame.
     *                NULL and {0, 0} both mean non-blocking.
     *                Time is expressed as relative CLOCK_MONOTONIC.
     *                As an optimization, if \p timeout->tv_sec is the maximum positive value for
     *                time_t (LONG_MAX), then the implementation treats it as infinite timeout.
     *
     * \return Actual number of frames available, if greater than or equal to zero.
     *         Guaranteed to be <= \p count.
     *
     *  \retval -EIO        corrupted indices, no recovery is possible
     *  \retval -EOVERFLOW  reader is not keeping up with writer (reader only)
     *  \retval -ETIMEDOUT  count is greater than zero, timeout is non-NULL and not {0, 0},
     *                      timeout expired, and no frames were available after the timeout.
     *  \retval -EINTR      count is greater than zero, timeout is non-NULL and not {0, 0}, timeout
     *                      was interrupted by a signal, and no frames were available after signal.
     *
     * Applications should treat all of these as equivalent to zero available frames,
     * except they convey extra information as to the cause.
     */
    virtual ssize_t obtain(audio_utils_iovec iovec[2], size_t count,
            const struct timespec *timeout = NULL) = 0;

    /**
     * Release access to a portion of the most recently obtained slice.
     * It is permitted to call release() multiple times without an intervening obtain().
     *
     * \param count Number of frames to release.  The total number of frames released must not
     *              exceed the number of frames most recently obtained.
     */
    virtual void release(size_t count) = 0;

    /**
     * Determine the number of frames that could be obtained or read/written without blocking.
     * There's an inherent race condition: the value may soon be obsolete so shouldn't be trusted.
     * available() may be called after obtain(), but doesn't affect the number of releasable frames.
     *
     * \return Number of available frames, if greater than or equal to zero.
     *  \retval -EIO        corrupted indices, no recovery is possible
     *  \retval -EOVERFLOW  reader is not keeping up with writer (reader only)
     */
    virtual ssize_t available() = 0;

protected:
    /** Number of frames obtained at most recent obtain(), less total number of frames released. */
    uint32_t    mObtained;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Used to write to a FIFO.  There should be exactly one writer per FIFO.
 * The writer is multi-thread safe with respect to reader(s),
 * but not with respect to multiple threads calling the writer API.
 */
class audio_utils_fifo_writer : public audio_utils_fifo_provider {

public:
    /**
     * Single-process and multi-process use same constructor here,
     * but different FIFO constructors.
     *
     * \param fifo Associated FIFO.  Passed by reference because it must be non-NULL.
     */
    audio_utils_fifo_writer(audio_utils_fifo& fifo);
    virtual ~audio_utils_fifo_writer();

    /**
     * Write to FIFO.  Resets the number of releasable frames to zero.
     *
     * \param buffer  Pointer to source buffer containing \p count frames of data.
     * \param count   Desired number of frames to write.
     * \param timeout Indicates the maximum time to block for at least one frame.
     *                NULL and {0, 0} both mean non-blocking.
     *                Time is expressed as relative CLOCK_MONOTONIC.
     *                As an optimization, if \p timeout->tv_sec is the maximum positive value for
     *                time_t (LONG_MAX), then the implementation treats it as infinite timeout.
     *
     * \return Actual number of frames written, if greater than or equal to zero.
     *         Guaranteed to be <= \p count.
     *         The actual transfer count may be zero if the FIFO is full,
     *         or partial if the FIFO was almost full.
     *  \retval -EIO       corrupted indices, no recovery is possible
     *  \retval -ETIMEDOUT count is greater than zero, timeout is non-NULL and not {0, 0},
     *                     timeout expired, and no frames were available after the timeout.
     *  \retval -EINTR     count is greater than zero, timeout is non-NULL and not {0, 0}, timeout
     *                     was interrupted by a signal, and no frames were available after signal.
     */
    ssize_t write(const void *buffer, size_t count, const struct timespec *timeout = NULL);

    // Implement audio_utils_fifo_provider
    virtual ssize_t obtain(audio_utils_iovec iovec[2], size_t count,
            const struct timespec *timeout = NULL);
    virtual void release(size_t count);
    virtual ssize_t available();

    /**
     * Set the current effective buffer size.
     * Any filled frames already written or released to the buffer are unaltered, and pending
     * releasable frames from obtain() may be release()ed.  However subsequent write() and obtain()
     * will be limited such that the total filled frame count is <= the effective buffer size.
     * The default effective buffer size is mFifo.mFrameCount.
     * Reducing the effective buffer size may update the hysteresis levels; see getHysteresis().
     *
     * \param frameCount    effective buffer size in frames. Capped to range [0, mFifo.mFrameCount].
     */
    void resize(uint32_t frameCount);

    /**
     * Get the current effective buffer size.
     *
     * \return effective buffer size in frames
     */
    uint32_t size() const;

    /**
     * Set the hysteresis levels for the writer to wake blocked readers.
     * A non-empty write() or release() will wake readers
     * only if the fill level was < \p lowLevelArm before the write() or release(),
     * and then the fill level became > \p highLevelTrigger afterwards.
     * The default value for \p lowLevelArm is mFifo.mFrameCount, which means always armed.
     * The default value for \p highLevelTrigger is zero,
     * which means every write() or release() will wake the readers.
     * For hysteresis, \p lowLevelArm must be <= \p highLevelTrigger + 1.
     * Increasing \p lowLevelArm will arm for wakeup, regardless of the current fill level.
     *
     * \param lowLevelArm       Arm for wakeup when fill level < this value.
     *                          Capped to range [0, effective buffer size].
     * \param highLevelTrigger  Trigger wakeup when armed and fill level > this value.
     *                          Capped to range [0, effective buffer size].
     */
    void setHysteresis(uint32_t lowLevelArm, uint32_t highLevelTrigger);

    /**
     * Get the hysteresis levels for waking readers.
     *
     * \param lowLevelArm       Set to the current low level arm value in frames.
     * \param highLevelTrigger  Set to the current high level trigger value in frames.
     */
    void getHysteresis(uint32_t *lowLevelArm, uint32_t *highLevelTrigger) const;

private:
    audio_utils_fifo&   mFifo;

    // Accessed by writer only using ordinary operations
    uint32_t    mLocalRear; // frame index of next frame slot available to write, or write index

    // TODO needs a state transition diagram for threshold and arming process
    // TODO make a separate class and associate with the synchronization object
    uint32_t    mLowLevelArm;       // arm if filled < arm level before release()
    uint32_t    mHighLevelTrigger;  // trigger if armed and filled > trigger level after release()
    bool        mArmed;             // whether currently armed

    uint32_t    mEffectiveFrames;   // current effective buffer size, <= mFifo.mFrameCount
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Used to read from a FIFO.  There can be one or more readers per FIFO,
 * and at most one of those readers can throttle the writer.
 * All other readers must keep up with the writer or they will lose frames.
 * Each reader is multi-thread safe with respect to the writer and any other readers,
 * but not with respect to multiple threads calling the reader API.
 */
class audio_utils_fifo_reader : public audio_utils_fifo_provider {

public:
    /**
     * Single-process and multi-process use same constructor here,
     * but different FIFO constructors.
     *
     * \param fifo Associated FIFO.  Passed by reference because it must be non-NULL.
     * \param throttlesWriter Whether this reader throttles the writer.
     *                        At most one reader can specify throttlesWriter == true.
     */
    audio_utils_fifo_reader(audio_utils_fifo& fifo, bool throttlesWriter = true);
    virtual ~audio_utils_fifo_reader();

    /**
     * Read from FIFO.  Resets the number of releasable frames to zero.
     *
     * \param buffer  Pointer to destination buffer to be filled with up to \p count frames of data.
     * \param count   Desired number of frames to read.
     * \param timeout Indicates the maximum time to block for at least one frame.
     *                NULL and {0, 0} both mean non-blocking.
     *                Time is expressed as relative CLOCK_MONOTONIC.
     *                As an optimization, if \p timeout->tv_sec is the maximum positive value for
     *                time_t (LONG_MAX), then the implementation treats it as infinite timeout.
     * \param lost    If non-NULL, updated to the approximate number of lost frames before re-sync.
     *
     * \return Actual number of frames read, if greater than or equal to zero.
     *         Guaranteed to be <= \p count.
     *         The actual transfer count may be zero if the FIFO is empty,
     *         or partial if the FIFO was almost empty.
     *  \retval -EIO        corrupted indices, no recovery is possible
     *  \retval -EOVERFLOW  reader is not keeping up with writer
     *  \retval -ETIMEDOUT  count is greater than zero, timeout is non-NULL and not {0, 0},
     *                      timeout expired, and no frames were available after the timeout.
     *  \retval -EINTR      count is greater than zero, timeout is non-NULL and not {0, 0}, timeout
     *                      was interrupted by a signal, and no frames were available after signal.
     */
    ssize_t read(void *buffer, size_t count, const struct timespec *timeout = NULL,
            size_t *lost = NULL);

    // Implement audio_utils_fifo_provider
    virtual ssize_t obtain(audio_utils_iovec iovec[2], size_t count,
            const struct timespec *timeout = NULL);
    virtual void release(size_t count);
    virtual ssize_t available();

    /**
     * Same as audio_utils_fifo_provider::obtain, except has an additional parameter \p lost.
     *
     * \param iovec   See audio_utils_fifo_provider::obtain.
     * \param count   See audio_utils_fifo_provider::obtain.
     * \param timeout See audio_utils_fifo_provider::obtain.
     * \param lost    If non-NULL, updated to the approximate number of lost frames before re-sync.
     * \return See audio_utils_fifo_provider::obtain for 'Returns' and 'Return values'.
     */
    ssize_t obtain(audio_utils_iovec iovec[2], size_t count, const struct timespec *timeout,
            size_t *lost);

    /**
     * Determine the number of frames that could be obtained or read without blocking.
     * There's an inherent race condition: the value may soon be obsolete so shouldn't be trusted.
     * available() may be called after obtain(), but doesn't affect the number of releasable frames.
     *
     * \param lost    If non-NULL, updated to the approximate number of lost frames before re-sync.
     *
     * \return Number of available frames, if greater than or equal to zero.
     *  \retval -EIO        corrupted indices, no recovery is possible
     *  \retval -EOVERFLOW  reader is not keeping up with writer
     */
    ssize_t available(size_t *lost);

    /**
     * Set the hysteresis levels for a throttling reader to wake a blocked writer.
     * A non-empty read() or release() by a throttling reader will wake the writer
     * only if the fill level was > \p highLevelArm before the read() or release(),
     * and then the fill level became < \p lowLevelTrigger afterwards.
     * The default value for \p highLevelArm is -1, which means always armed.
     * The default value for \p lowLevelTrigger is mFifo.mFrameCount,
     * which means every read() or release() will wake the writer.
     * For hysteresis, \p highLevelArm must be >= \p lowLevelTrigger - 1.
     * Decreasing \p highLevelArm will arm for wakeup, regardless of the current fill level.
     * Note that the throttling reader is not directly aware of the writer's effective buffer size,
     * so any change in effective buffer size must be communicated indirectly.
     *
     * \param highLevelArm      Arm for wakeup when fill level > this value.
     *                          Capped to range [-1, mFifo.mFrameCount].
     * \param lowLevelTrigger   Trigger wakeup when armed and fill level < this value.
     *                          Capped to range [0, mFifo.mFrameCount].
     */
    void setHysteresis(int32_t highLevelArm, uint32_t lowLevelTrigger);

    /**
     * Get the hysteresis levels for waking readers.
     *
     * \param highLevelArm      Set to the current high level arm value in frames.
     * \param lowLevelTrigger   Set to the current low level trigger value in frames.
     */
    void getHysteresis(int32_t *highLevelArm, uint32_t *lowLevelTrigger) const;

private:
    audio_utils_fifo&   mFifo;

    // Accessed by reader only using ordinary operations
    uint32_t     mLocalFront;   // frame index of first frame slot available to read, or read index

    // Points to shared front index if this reader throttles writer, or NULL if we don't throttle
    // FIXME consider making it a boolean
    audio_utils_fifo_index*     mThrottleFront;

    // TODO not used yet, needs state transition diagram
    int32_t     mHighLevelArm;      // arm if filled > arm level before release()
    uint32_t    mLowLevelTrigger;   // trigger if armed and filled < trigger level after release()
    bool        mArmed;             // whether currently armed
};

#endif  // !ANDROID_AUDIO_FIFO_H
