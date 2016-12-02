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

#include <audio_utils/fifo_index.h>
#include <audio_utils/futex.h>

// These are not implemented within <audio_utils/fifo_index.h>
// so that we don't expose futex.

uint32_t audio_utils_fifo_index::loadAcquire()
{
    return atomic_load_explicit(&mIndex, std::memory_order_acquire);
}

void audio_utils_fifo_index::storeRelease(uint32_t value)
{
    atomic_store_explicit(&mIndex, value, std::memory_order_release);
}

int audio_utils_fifo_index::wait(int op, uint32_t expected, const struct timespec *timeout)
{
    return sys_futex(&mIndex, op, expected, timeout, NULL, 0);
}

int audio_utils_fifo_index::wake(int op, int waiters)
{
    return sys_futex(&mIndex, op, waiters, NULL, NULL, 0);
}
