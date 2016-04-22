// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Handler for input events in /dev/input. AudioVolumeHandler handles events
// only for volume key presses.

#ifndef BRILLO_AUDIO_AUDIOSERVICE_AUDIO_VOLUME_HANDLER_H_
#define BRILLO_AUDIO_AUDIOSERVICE_AUDIO_VOLUME_HANDLER_H_

#include <base/files/file_path.h>
#include <brillo/key_value_store.h>
#include <gtest/gtest_prod.h>
#include <linux/input.h>
#include <media/IAudioPolicyService.h>
#include <system/audio.h>

#include "audio_daemon_handler.h"

namespace brillo {

class AudioVolumeHandler : public AudioDaemonHandler {
 public:
  AudioVolumeHandler();
  virtual ~AudioVolumeHandler();

  // Get the current state of the headset jack and update AudioSystem based on
  // the initial state.
  //
  // |aps| is a pointer to the binder object.
  virtual void Init(android::sp<android::IAudioPolicyService> aps) override;

  // Process input events from the kernel. Connecting/disconnecting an audio
  // device will result in multiple calls to this method.
  //
  // |event| is a pointer to an input_event. This function should be able to
  // gracefully handle input events that are not relevant to the functionality
  // provided by this class.
  virtual void ProcessEvent(const struct input_event& event) override;

  // Inform the handler that the audio policy service has been disconnected.
  virtual void APSDisconnect() override;

  // Inform the handler that the audio policy service is reconnected.
  //
  // |aps| is a pointer to the binder object.
  virtual void APSConnect(
      android::sp<android::IAudioPolicyService> aps) override;

  // Set the stream to use when volume buttons are pressed.
  //
  // |stream| is an int representing the stream. Passing STREAM_DEFAULT to this
  // method can be used to reset selected_stream_.
  void SetVolumeControlStream(audio_stream_type_t stream);

 private:
  friend class AudioVolumeHandlerTest;
  FRIEND_TEST(AudioVolumeHandlerTest, FileGeneration);
  FRIEND_TEST(AudioVolumeHandlerTest, GetVolumeForKey);
  FRIEND_TEST(AudioVolumeHandlerTest, GetVolumeForStreamDeviceTuple);
  FRIEND_TEST(AudioVolumeHandlerTest, SetVolumeForStreamDeviceTuple);
  FRIEND_TEST(AudioVolumeHandlerTest, InitNoFile);
  FRIEND_TEST(AudioVolumeHandlerTest, InitFilePresent);
  FRIEND_TEST(AudioVolumeHandlerTest, ProcessEventEmpty);
  FRIEND_TEST(AudioVolumeHandlerTest, ProcessEventKeyUp);
  FRIEND_TEST(AudioVolumeHandlerTest, ProcessEventKeyDown);
  FRIEND_TEST(AudioVolumeHandlerTest, SelectStream);
  FRIEND_TEST(AudioVolumeHandlerTest, ComputeNewVolume);

  // Read the initial volume of audio streams.
  //
  // |path| is the file that contains the initial volume state.
  void GetInitialVolumeState(const base::FilePath& path);

  // Get the volume of a given key.
  //
  // |stream| is an int representing the stream.
  // |device| is an int representing the device.
  //
  // Returns an int which corresponds to the current index.
  int GetVolumeCurrentIndex(audio_stream_type_t stream, audio_devices_t device);

  // Get the volume of a given key.
  //
  // |key| is a string representing key in the key-value file.
  //
  // Returns an int which corresponds to the index.
  int GetVolumeForKey(const std::string& key);

  // Set the volume for a given (stream, device) tuple.
  //
  // |stream| is an int representing the stream.
  // |device| is an int representing the device.
  // |index| is an int representing the volume.
  void SetVolumeCurrentIndex(audio_stream_type_t stream, audio_devices_t device,
                             int index);

  // Update the volume index for a given stream.
  //
  // |previous_index| is the current index of the stream/device tuple before the
  // volume button is pressed.
  // |direction| is an int which is multiplied to step_. +1 for volume up and -1
  // for volume down.
  // |stream| is an int representing the stream.
  //
  // Returns the new volume index.
  int GetNewVolumeIndex(int previous_index, int direction,
                        audio_stream_type_t stream);

  // Adjust the volume of the active streams in the direction indicated. If
  // SetDefaultStream() is called, then only the volume for that stream will be
  // changed.
  //
  // |direction| is an int which is multiplied to step_. +1 for volume up and -1
  // for volume down.
  virtual void AdjustVolumeActiveStreams(int direction);

  // Adjust the volume of a given stream in the direction specified.
  //
  // |stream| is an int representing the stream.
  // |direction| is an int which is multiplied to step_. +1 for volume up and -1
  // for volume down.
  void AdjustStreamVolume(audio_stream_type_t stream, int direction);

  // Set the file path for testing.
  //
  // |path| to use while running tests.
  void SetVolumeFilePathForTesting(const base::FilePath& path);

  // Initialize all the streams in the audio policy service.
  virtual void InitAPSAllStreams();

  // Generate the volume config file.
  void GenerateVolumeFile();

  // Stream to use for volume control.
  audio_stream_type_t selected_stream_;
  // File backed key-value store of the current index (as seen by the audio
  // policy service).
  std::unique_ptr<KeyValueStore> kv_store_;
  // Supported stream names. The order of this vector defines the priority from
  // high to low.
  std::vector<audio_stream_type_t> kSupportedStreams_{
      AUDIO_STREAM_ALARM, AUDIO_STREAM_NOTIFICATION, AUDIO_STREAM_SYSTEM,
      AUDIO_STREAM_MUSIC};
  // Step size for each stream. This is used to translate between user defined
  // stream ranges and the range as seen by audio policy service. This value is
  // not file-backed and is intended to be re-applied by the user on reboots and
  // brilloaudioservice crashes.
  std::map<audio_stream_type_t, int> step_sizes_;
  // Key indicies.
  const std::string kCurrentIndexKey_ = "current_index";
  // Default values.
  const int kMinIndex_ = 0;
  const int kDefaultCurrentIndex_ = 30;
  const int kMaxIndex_ = 100;
  const int kDefaultStepSize_ = 1;
  base::FilePath volume_state_file_;
};

}  // namespace brillo

#endif  // BRILLO_AUDIO_AUDIOSERVICE_AUDIO_VOLUME_HANDLER_H_
