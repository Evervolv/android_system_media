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

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <audio_utils/fifo.h>
extern "C" {
#include "getch.h"
}

struct Context {
    audio_utils_fifo_writer *mInputWriter;
    audio_utils_fifo_reader *mInputReader;
    audio_utils_fifo_writer *mTransferWriter;
    audio_utils_fifo_reader *mTransferReader;
    audio_utils_fifo_writer *mOutputWriter;
    audio_utils_fifo_reader *mOutputReader;
};

void *input_routine(void *arg)
{
    Context *context = (Context *) arg;
    for (;;) {
        struct timespec timeout;
        timeout.tv_sec = 3;
        timeout.tv_nsec = 0;
        char buffer[4];
        ssize_t actual = context->mInputReader->read(buffer, sizeof(buffer), &timeout);
        if (actual == 0) {
            (void) write(1, "t", 1);
        } else if (actual > 0) {
            actual = context->mTransferWriter->write(buffer, actual, &timeout);
            //printf("transfer.write actual = %d\n", (int) actual);
        } else {
            printf("input.read actual = %d\n", (int) actual);
        }
    }
    return NULL;
}

void *output_routine(void *arg)
{
    Context *context = (Context *) arg;
    for (;;) {
        struct timespec timeout;
        timeout.tv_sec = 5;
        timeout.tv_nsec = 0;
        char buffer[4];
        ssize_t actual = context->mTransferReader->read(buffer, sizeof(buffer), &timeout);
        if (actual == 0) {
            (void) write(1, "T", 1);
        } else if (actual > 0) {
            actual = context->mOutputWriter->write(buffer, actual, &timeout);
            //printf("output.write actual = %d\n", (int) actual);
        } else {
            printf("transfer.read actual = %d\n", (int) actual);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    set_conio_terminal_mode();
    argc = argc + 0;
    argv = &argv[0];

    char inputBuffer[64];
    audio_utils_fifo inputFifo(sizeof(inputBuffer) /*frameCount*/, 1 /*frameSize*/, inputBuffer);
    audio_utils_fifo_writer inputWriter(inputFifo);
    audio_utils_fifo_reader inputReader(inputFifo, true /*readerThrottlesWriter*/);
    inputWriter.setHighLevelTrigger(3);

    char transferBuffer[64];
    audio_utils_fifo transferFifo(sizeof(transferBuffer) /*frameCount*/, 1 /*frameSize*/,
            transferBuffer);
    audio_utils_fifo_writer transferWriter(transferFifo);
    audio_utils_fifo_reader transferReader(transferFifo, true /*readerThrottlesWriter*/);
    transferWriter.setEffectiveFrames(2);

    char outputBuffer[64];
    audio_utils_fifo outputFifo(sizeof(outputBuffer) /*frameCount*/, 1 /*frameSize*/, outputBuffer);
    audio_utils_fifo_writer outputWriter(outputFifo);
    audio_utils_fifo_reader outputReader(outputFifo, true /*readerThrottlesWriter*/);

    Context context;
    context.mInputWriter = &inputWriter;
    context.mInputReader = &inputReader;
    context.mTransferWriter = &transferWriter;
    context.mTransferReader = &transferReader;
    context.mOutputWriter = &outputWriter;
    context.mOutputReader = &outputReader;

    pthread_t input_thread;
    int ok = pthread_create(&input_thread, (const pthread_attr_t *) NULL, input_routine,
            (void *) &context);
    pthread_t output_thread;
    ok = pthread_create(&output_thread, (const pthread_attr_t *) NULL, output_routine,
            (void *) &context);
    ok = ok + 0;

    for (;;) {
        char buffer[1];
        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;
        ssize_t actual = outputReader.read(buffer, sizeof(buffer), &timeout);
        if (actual == 1) {
            printf("%c", buffer[0]);
            fflush(stdout);
        } else if (actual != 0) {
            printf("outputReader.read actual = %d\n", (int) actual);
        }
        if (kbhit()) {
            int ch = getch();
            if (ch <= 0 || ch == 3) {
                break;
            }
            buffer[0] = ch;
            actual = inputWriter.write(buffer, sizeof(buffer), &timeout);
            if (actual != 1) {
                printf("inputWriter.write actual = %d\n", (int) actual);
            }
        }
    }
    reset_terminal_mode();
}
