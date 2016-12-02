/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_FIFO_INDEX_H
#define ANDROID_AUDIO_FIFO_INDEX_H

#include <atomic>
#include <stdint.h>
#include <time.h>

/**
 * An index that may optionally be placed in shared memory.
 * Must be Plain Old Data (POD), so no virtual methods are allowed.
 * If in shared memory, exactly one process must explicitly call the constructor via placement new.
 * \see #audio_utils_fifo_sync
 */
class audio_utils_fifo_index {

public:
    audio_utils_fifo_index() : mIndex(0) { }
    ~audio_utils_fifo_index() { }

    uint32_t loadAcquire();
    void storeRelease(uint32_t value);
    int wait(int op, uint32_t expected, const struct timespec *timeout);
    int wake(int op, int waiters);

private:
    // Linux futex is 32 bits regardless of platform.
    // It would make more sense to declare this as atomic_uint32_t, but there is no such type name.
    // TODO Support 64-bit index with 32-bit futex in low-order bits.
    std::atomic_uint_least32_t  mIndex; // accessed by both sides using atomic operations
    static_assert(sizeof(mIndex) == sizeof(uint32_t), "mIndex must be 32 bits");
};

static_assert(sizeof(audio_utils_fifo_index) == sizeof(uint32_t),
        "audio_utils_fifo_index must be 32 bits");

#endif  // !ANDROID_AUDIO_FIFO_INDEX_H
