/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_CLOCK_H
#define ANDROID_AUDIO_CLOCK_H

#include <time.h>

/**
 * \brief Converts time in ns to a time string, with format similar to logcat.
 * \param ns          input time in nanoseconds to convert.
 * \param buffer      caller allocated string buffer, buffer_length must be >= 19 chars
 *                    in order to fully fit in time.  The string is always returned
 *                    null terminated if buffer_size is greater than zero.
 * \param buffer_size size of buffer.
 */
static inline void audio_utils_ns_to_string(int64_t ns, char *buffer, int buffer_size)
{
    const int one_second = 1000000000;
    const time_t sec = ns / one_second;
    struct tm tm;
    localtime_r(&sec, &tm);
    if (snprintf(buffer, buffer_size, "%02d-%02d %02d:%02d:%02d.%03d",
        tm.tm_mon + 1, // localtime_r uses months in 0 - 11 range
        tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
        (int)(ns % one_second / 1000000)) < 0) {
        buffer[0] = '\0'; // null terminate on format error, which should not happen
    }
}

/**
 * \brief Converts a timespec to nanoseconds.
 * \param ts   input timespec to convert.
 * \return     timespec converted to nanoseconds.
 */
static inline int64_t audio_utils_ns_from_timespec(const struct timespec *ts)
{
    return ts->tv_sec * 1000000000LL + ts->tv_nsec;
}

/**
 * \brief Gets the real time clock in nanoseconds.
 * \return the real time clock in nanoseconds, or 0 on error.
 */
static inline int64_t audio_utils_get_real_time_ns() {
    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == 0) {
        return audio_utils_ns_from_timespec(&now_ts);
    }
    return 0; // should not happen.
}

#endif  // !ANDROID_AUDIO_CLOCK_H
