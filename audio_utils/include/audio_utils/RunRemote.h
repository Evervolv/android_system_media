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
#include <functional>
#include <sys/wait.h>
#include <unistd.h>

namespace android::audio_utils {

/**
 * RunRemote run a method in a remote process.
 *
 * This can be used for lightweight remote process testing.
 * This can be used for implementing microservices.
 */
class RunRemote {
public:
    /** Runs the method without a communication pipe. */
    explicit RunRemote(std::function<void()>&& runnable, bool detached = false)
            : mRunnable{std::move(runnable)}
            , mDetached(detached) {}

    /** Runs the method with a reference back to the RunRemote for communication */
    explicit RunRemote(
            std::function<void(RunRemote& runRemote)>&& runnable, bool detached = false)
            : mRunnableExt{std::move(runnable)}
            , mDetached(detached) {}

    ~RunRemote() {
        if (!mDetached) stop();
    }

    bool run() {
        int fd1[2] = {-1, -1};
        int fd2[2] = {-1, -1};
        if (mRunnableExt) {
            if (pipe(fd1) != 0) return false;
            if (pipe(fd2) != 0) {
                close(fd1[0]);
                close(fd1[1]);
                return false;
            }
        }
        pid_t ret = fork();
        if (ret < 0) {
            close(fd1[0]);
            close(fd1[1]);
            close(fd2[0]);
            close(fd2[1]);
            return false;
        } else if (ret == 0) {
            // child
            if (mRunnableExt) {
                mInFd = fd2[0];
                close(fd2[1]);
                mOutFd = fd1[1];
                close(fd1[0]);
                mRunnableExt(*this);
            } else {
                mRunnable();
            }
            // let the system reclaim handles.
            exit(EXIT_SUCCESS);
        } else {
            // parent
            if (mRunnableExt) {
                mInFd = fd1[0];
                close(fd1[1]);
                mOutFd = fd2[1];
                close(fd2[0]);
            }
            mChildPid = ret;
            return true;
        }
    }

    bool stop() {
        if (mInFd != -1) {
            close(mInFd);
            mInFd = -1;
        }
        if (mOutFd != -1) {
            close(mOutFd);
            mOutFd = -1;
        }
        if (!mDetached && mChildPid > 0) {
            if (kill(mChildPid, SIGTERM)) {
                return false;
            }
            int status = 0;
            if (TEMP_FAILURE_RETRY(waitpid(mChildPid, &status, 0)) != mChildPid) {
                return false;
            }
            mChildPid = 0;
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        return true;
    }

    /** waits for a char from the remote process. */
    int getChar() {
        unsigned char c;
        // EOF returns 0 (this is a blocking read), -1 on error.
        if (read(mInFd, &c, 1) != 1) return -1;
        return c;
    }

    /** sends a char to the remote process. */
    int putChar(int c) {
        while (true) {
            int ret = write(mOutFd, &c, 1);  // LE.
            if (ret == 1) return 1;
            if (ret < 0) return -1;
            // on 0, retry.
        }
    }

private:
    const std::function<void()> mRunnable;
    const std::function<void(RunRemote& runRemote)> mRunnableExt;
    const bool mDetached;

    // These values are effectively const after calling run(), which does the fork,
    // until stop() is called, which terminates the remote process.  run() is assumed
    // called shortly after construction, and not asynchronously with a reader or writer.

    pid_t mChildPid = 0;
    int mOutFd = -1;
    int mInFd =1;
};

}  // namespace android::audio_utils
