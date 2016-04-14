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

#ifndef BRILLO_AUDIO_AUDIOSERVICE_BRILLO_AUDIO_SERVICE_H_
#define BRILLO_AUDIO_AUDIOSERVICE_BRILLO_AUDIO_SERVICE_H_

#include "android/brillo/brilloaudioservice/BnBrilloAudioService.h"

#include <memory>
#include <set>
#include <vector>

#include <binder/Status.h>

#include "audio_device_handler.h"
#include "android/brillo/brilloaudioservice/IAudioServiceCallback.h"

using android::binder::Status;
using android::brillo::brilloaudioservice::BnBrilloAudioService;
using android::brillo::brilloaudioservice::IAudioServiceCallback;

namespace brillo {

class BrilloAudioService : public BnBrilloAudioService {
 public:
  // From AIDL.
  Status GetDevices(int flag, std::vector<int>* _aidl_return);
  Status SetDevice(int usage, int config);
  Status RegisterServiceCallback(
      const android::sp<IAudioServiceCallback>& callback);
  Status UnregisterServiceCallback(
      const android::sp<IAudioServiceCallback>& callback);

  // Register a device handler.
  //
  // |audio_device_handler| is a weak pointer to an audio device handler object.
  void RegisterDeviceHandler(
      std::weak_ptr<AudioDeviceHandler> audio_device_handler);

  // Callback to be called when a device is connected.
  //
  // |devices| is a vector of ints representing the audio_devices_t.
  void OnDevicesConnected(const std::vector<int>& device);

  // Callback to be called when a device is disconnected.
  //
  // |devices| is a vector of ints representing the audio_devices_t.
  void OnDevicesDisconnected(const std::vector<int>& device);

 private:
  // A weak pointer to the audio device handler.
  std::weak_ptr<AudioDeviceHandler> audio_device_handler_;
  // List of all callbacks objects registered with the service.
  std::set<android::sp<IAudioServiceCallback> > callbacks_set_;
};

}  // namespace brillo

#endif  // BRILLO_AUDIO_AUDIOSERVICE_BRILLO_AUDIO_SERVICE_H_
