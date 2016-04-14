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

#ifndef BRILLO_AUDIO_AUDIOSERVICE_BRILLO_AUDIO_SERVICE_MOCK_H_
#define BRILLO_AUDIO_AUDIOSERVICE_BRILLO_AUDIO_SERVICE_MOCK_H_

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest_prod.h>

#include "brillo_audio_service.h"

namespace brillo {

class BrilloAudioServiceMock : public BrilloAudioService {
 public:
  BrilloAudioServiceMock() = default;
  ~BrilloAudioServiceMock() {}

  MOCK_METHOD2(GetDevices, Status(int flag, std::vector<int>* _aidl_return));
  MOCK_METHOD2(SetDevice, Status(int usage, int config));
  MOCK_METHOD1(RegisterServiceCallback,
               Status(const android::sp<IAudioServiceCallback>& callback));
  MOCK_METHOD1(UnregisterServiceCallback,
               Status(const android::sp<IAudioServiceCallback>& callback));

  void RegisterDeviceHandler(std::weak_ptr<AudioDeviceHandler>){};
  void OnDevicesConnected(const std::vector<int>&) {}
  void OnDevicesDisconnected(const std::vector<int>&) {}
};

}  // namespace brillo

#endif  // BRILLO_AUDIO_AUDIOSERVICE_BRILLO_AUDIO_SERVICE_MOCK_H_
