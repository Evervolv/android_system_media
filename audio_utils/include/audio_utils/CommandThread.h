/*
 * Copyright (C) 2024 The Android Open Source Project
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

#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <audio_utils/mutex.h>

namespace android::audio_utils {

/**
 * CommandThread is used for serial execution of commands
 * on a single worker thread.
 *
 * This class is thread-safe.
 */

class CommandThread {
public:
    CommandThread() {
        // threadLoop() should be started after the class is initialized.
        mThread = std::thread([this](){this->threadLoop();});
    }

    ~CommandThread() {
        quit();
        mThread.join();
    }

    /**
     * Add a command to the command queue.
     *
     * If the func is a closure containing references, suggest using shared_ptr
     * instead to maintain proper lifetime.
     *
     * @param name for dump() purposes.
     * @param func command to execute
     */
    void add(std::string_view name, std::function<void()>&& func) {
        std::lock_guard lg(mMutex);
        if (mQuit) return;
        mCommands.emplace_back(name, std::move(func));
        if (mCommands.size() == 1) mConditionVariable.notify_one();
    }

    /**
     * Returns the string of commands, separated by newlines.
     */
    std::string dump() const {
        std::string result;
        std::lock_guard lg(mMutex);
        for (const auto &p : mCommands) {
            result.append(p.first).append("\n");
        }
        return result;
    }

    /**
     * Quits the command thread and empties the command queue.
     */
    void quit() {
        std::lock_guard lg(mMutex);
        if (mQuit) return;
        mQuit = true;
        mCommands.clear();
        mConditionVariable.notify_one();
    }

    /**
     * Returns the number of commands on the queue.
     */
    size_t size() const {
        std::lock_guard lg(mMutex);
        return mCommands.size();
    }

private:
    std::thread mThread;
    mutable std::mutex mMutex;
    std::condition_variable mConditionVariable GUARDED_BY(mMutex);
    std::deque<std::pair<std::string, std::function<void()>>> mCommands GUARDED_BY(mMutex);
    bool mQuit GUARDED_BY(mMutex) = false;

    void threadLoop() {
        audio_utils::unique_lock ul(mMutex);
        while (!mQuit) {
            if (!mCommands.empty()) {
                auto name = std::move(mCommands.front().first);
                auto func = std::move(mCommands.front().second);
                mCommands.pop_front();
                ul.unlock();
                // ALOGD("%s: executing %s", __func__, name.c_str());
                func();
                ul.lock();
                continue;
            }
            mConditionVariable.wait(ul);
        }
    }
};

}  // namespace android::audio_utils
