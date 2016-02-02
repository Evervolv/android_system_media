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


package android.brillo.brilloaudioservice;

import android.brillo.brilloaudioservice.IAudioServiceCallback;

/*
 * Interface for BrilloAudioService that clients can use to get the list of
 * devices currently connected to the system as well as to register callbacks to
 * be notified when the device state changes.
 */
interface IBrilloAudioService {
  const int GET_DEVICES_INPUTS = 1;
  const int GET_DEVICES_OUTPUTS = 2;

  // Get the list of devices connected. If flag is GET_DEVICES_INPUTS, then
  // return input devices. Otherwise, return output devices.
  int[] GetDevices(int flag);

  // Register a callback object with the service.
  void RegisterServiceCallback(IAudioServiceCallback callback);

  // Unregister a callback object.
  void UnregisterServiceCallback(IAudioServiceCallback callback);
}
