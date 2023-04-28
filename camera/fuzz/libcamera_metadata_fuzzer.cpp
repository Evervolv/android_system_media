/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android/hardware/camera/device/3.2/ICameraDeviceSession.h>
#include <android/hardware/camera/provider/2.7/ICameraProvider.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <signal.h>
#include <system/camera_metadata.h>
#include <system/camera_vendor_tags.h>
#include <utils/Vector.h>

using ::android::sp;
using ::android::Vector;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::device::V3_2::BufferCache;
using ::android::hardware::camera::device::V3_2::CaptureRequest;
using ::android::hardware::camera::device::V3_2::CaptureResult;
using ::android::hardware::camera::device::V3_2::ICameraDevice;
using ::android::hardware::camera::device::V3_2::ICameraDeviceCallback;
using ::android::hardware::camera::device::V3_2::ICameraDeviceSession;
using ::android::hardware::camera::device::V3_2::NotifyMsg;
using ::android::hardware::camera::provider::V2_7::ICameraProvider;

static constexpr uint32_t kMinNumEntries = 0;
static constexpr uint32_t kMaxNumEntries = 1000;
static constexpr int32_t kMinAPIcase = 0;
static constexpr int32_t kMaxcameraMetadataAPIs = 12;
static constexpr int32_t kMinDataCount = 1;
static constexpr int32_t kMaxDataCount = 3;
static std::string kServiceName = "internal/0";
static std::string kVersion[] = {"0", "1", "2", "3", "4",
                                 "5", "6", "7", "8", "9"};

#ifndef __unused
#define __unused __attribute__((unused))
#endif

typedef uint32_t metadata_uptrdiff_t;
typedef uint32_t metadata_size_t;
typedef uint64_t metadata_vendor_id_t;

struct camera_metadata {
  metadata_size_t size;
  uint32_t version;
  uint32_t flags;
  metadata_size_t entry_count;
  metadata_size_t entry_capacity;
  metadata_uptrdiff_t entries_start; // Offset from camera_metadata
  metadata_size_t data_count;
  metadata_size_t data_capacity;
  metadata_uptrdiff_t data_start; // Offset from camera_metadata
  uint32_t padding;               // padding to 8 bytes boundary
  metadata_vendor_id_t vendor_id;
};

template <typename T> const hidl_vec<T> toHidlVec(const Vector<T> &Vector) {
  hidl_vec<T> vec;
  vec.setToExternal(const_cast<T *>(Vector.array()), Vector.size());
  return vec;
}

class FuzzICameraDeviceCallback : public ICameraDeviceCallback {
public:
  Return<void>
  processCaptureResult(const hidl_vec<CaptureResult> &results __unused) {
    return Void();
  }

  Return<void> notify(const hidl_vec<NotifyMsg> &msgs __unused) {
    return Void();
  }
};

class CameraMetadataFuzzer {
public:
  CameraMetadataFuzzer(const uint8_t *data, size_t size) : mFdp(data, size){};
  void invokeCameraMetadataAPI();

private:
  FuzzedDataProvider mFdp;
  void invokeProcessCaptureRequest();
  camera_metadata_t *createMetadata();
  void invokeAddCameraMetadata(camera_metadata_t* metadata, uint32_t tag);
};

void CameraMetadataFuzzer::invokeProcessCaptureRequest() {
  sp<ICameraProvider> cameraProvider =
      ICameraProvider::getService(kServiceName.c_str());
  if (cameraProvider) {
    sp<ICameraDevice> cameraDevice = nullptr;
    std::string version1 = mFdp.PickValueInArray(kVersion);
    std::string version2 = mFdp.PickValueInArray(kVersion);
    std::string deviceInterface =
        "device@" + version1 + "." + version2 + "/" + kServiceName;
    cameraProvider->getCameraDeviceInterface_V3_x(
        hidl_string(deviceInterface.c_str()),
        [&](Status status, const sp<ICameraDevice> &device3_x) {
          if (status == Status::OK) {
            cameraDevice = device3_x;
          }
        });
    if (cameraDevice) {
      sp<ICameraDeviceCallback> cameraDeviceCallback =
          new FuzzICameraDeviceCallback();

      sp<ICameraDeviceSession> cameraDeviceSession;
      cameraDevice->open(
          cameraDeviceCallback,
          [&](Status status, const sp<ICameraDeviceSession> &session) {
            if (status == Status::OK) {
              cameraDeviceSession = session;
            }
          });

      CaptureRequest request = {};
      struct camera_metadata metadata {};
      metadata.data_capacity =
          mFdp.ConsumeIntegralInRange<size_t>(kMinNumEntries, kMaxNumEntries);
      metadata.data_count = mFdp.ConsumeIntegral<size_t>();
      metadata.data_start = mFdp.ConsumeIntegralInRange<size_t>(
          sizeof(struct camera_metadata), UINT64_MAX);
      metadata.size = mFdp.ConsumeIntegral<size_t>();
      metadata.entry_count = mFdp.ConsumeIntegral<size_t>();
      metadata.entry_capacity =
          mFdp.ConsumeIntegralInRange<size_t>(kMinNumEntries, kMaxNumEntries);
      metadata.entries_start = mFdp.ConsumeIntegralInRange<size_t>(
          sizeof(struct camera_metadata), UINT64_MAX);
      metadata.vendor_id = mFdp.ConsumeBool()
          ? mFdp.ConsumeIntegral<metadata_vendor_id_t>()
          : CAMERA_METADATA_INVALID_VENDOR_ID;
      metadata.version = mFdp.ConsumeIntegral<uint32_t>();
      metadata.flags = mFdp.ConsumeIntegral<uint32_t>();

      request.settings.setToExternal((unsigned char *)&metadata,
                                     sizeof(metadata));
      Vector<CaptureRequest> requests;
      requests.push_back(request);
      hidl_vec<BufferCache> cachesToRemove;

      Status status = Status::INTERNAL_ERROR;
      uint32_t numRequestProcessed = 0;
      auto resultCallback = [&status, &numRequestProcessed](auto s,
                                                            uint32_t n) {
        status = s;
        numRequestProcessed = n;
      };

      cameraDeviceSession->processCaptureRequest(
          toHidlVec(requests), cachesToRemove, resultCallback);
    }
  }
}

camera_metadata_t *CameraMetadataFuzzer::createMetadata() {
  uint32_t entry_capacity =
      mFdp.ConsumeIntegralInRange<size_t>(kMinNumEntries, kMaxNumEntries);
  uint32_t data_capacity =
      mFdp.ConsumeIntegralInRange<size_t>(kMinNumEntries, kMaxNumEntries);
  camera_metadata_t *metadata =
      allocate_camera_metadata(entry_capacity, data_capacity);
  return metadata;
}

void CameraMetadataFuzzer::invokeAddCameraMetadata(camera_metadata_t* metadata, uint32_t tag) {
    int32_t type = get_camera_metadata_tag_type(tag);
    uint32_t data_count = mFdp.ConsumeIntegralInRange<size_t>(kMinDataCount, kMaxDataCount);
    void* tag_data = nullptr;
    switch (type) {
        case TYPE_BYTE: {
            uint8_t data[data_count];
            for (int i = 0; i < data_count; ++i) {
                data[i] = mFdp.ConsumeIntegral<uint8_t>();
            }
            tag_data = &data;
             break;
        }
        case TYPE_INT32: {
            int32_t data[data_count];
            for (int i = 0; i < data_count; ++i) {
                data[i] = mFdp.ConsumeIntegral<int32_t>();
            }
            tag_data = &data;
            break;
        }
        case TYPE_FLOAT: {
            float data[data_count];
            for (int i = 0; i < data_count; ++i) {
                data[i] = mFdp.ConsumeFloatingPoint<float>();
            }
            tag_data = &data;
            break;
        }
        case TYPE_INT64: {
            int64_t data[data_count];
            for (int i = 0; i < data_count; ++i) {
                data[i] = mFdp.ConsumeIntegral<int64_t>();
            }
            tag_data = &data;
            break;
        }
        case TYPE_DOUBLE: {
            double data[data_count];
            for (int i = 0; i < data_count; ++i) {
                data[i] = mFdp.ConsumeFloatingPoint<float>();
            }
            tag_data = &data;
            break;
        }
        case TYPE_RATIONAL:
        default: {
            camera_metadata_rational data[data_count];
            for (int i = 0; i < data_count; ++i) {
                data[i].numerator = mFdp.ConsumeIntegral<int32_t>();
                data[i].denominator = mFdp.ConsumeIntegral<int32_t>();
            }
            tag_data = &data;
            break;
        }
    }
    add_camera_metadata_entry(metadata, tag, tag_data, data_count);
}

void CameraMetadataFuzzer::invokeCameraMetadataAPI() {
  while (mFdp.remaining_bytes() > 0) {
    int32_t cameraMetadataAPI = mFdp.ConsumeIntegralInRange<size_t>(
        kMinAPIcase, kMaxcameraMetadataAPIs);
    camera_metadata_t *metadata = createMetadata();
    uint32_t tag = mFdp.ConsumeIntegral<uint32_t>();
    camera_metadata_entry_t entry;
    switch (cameraMetadataAPI) {
       case 0: {
           invokeAddCameraMetadata(metadata, tag);
           break;
       }
       case 1: {
           size_t size = mFdp.ConsumeIntegral<size_t>();
           validate_camera_metadata_structure(metadata, &size);
           break;
       }
       case 2: {
           find_camera_metadata_ro_entry(metadata, tag,
                                    (camera_metadata_ro_entry_t *)&entry);
           break;
       }
       case 3: {
           uint32_t index = mFdp.ConsumeIntegral<uint32_t>();
           delete_camera_metadata_entry(metadata, index);
           break;
       }
       case 4: {
           get_camera_metadata_section_name(tag);
           break;
       }
       case 5: {
           get_camera_metadata_tag_name(tag);
           break;
       }
       case 6: {
           get_camera_metadata_tag_type(tag);
           break;
       }
       case 7: {
           size_t index = mFdp.ConsumeIntegral<size_t>();
           uint32_t data = mFdp.ConsumeIntegral<uint32_t>();
           uint32_t dataCount = mFdp.ConsumeIntegral<uint32_t>();
           camera_metadata_t *dstMetadata = createMetadata();
           update_camera_metadata_entry(dstMetadata, index, &data, dataCount,
                                   &entry);
           free_camera_metadata(dstMetadata);
           break;
       }
       case 8: {
           size_t index = mFdp.ConsumeIntegral<size_t>();
           get_camera_metadata_ro_entry(metadata, index,
                                   (camera_metadata_ro_entry_t *)&entry);
           break;
       }
       case 9: {
           sort_camera_metadata(metadata);
           break;
       }
       case 10: {
           camera_metadata_t *cloneMetadata = clone_camera_metadata(metadata);
           free_camera_metadata(cloneMetadata);
           break;
       }
       case 11: {
           invokeProcessCaptureRequest();
           break;
       }
       default: {
           size_t dst_size = mFdp.ConsumeIntegral<size_t>();
           camera_metadata_t *dstMetadata = createMetadata();
           copy_camera_metadata(dstMetadata, dst_size, metadata);
           free_camera_metadata(dstMetadata);
           break;
       }
    }
    free_camera_metadata(metadata);
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  CameraMetadataFuzzer cameraMetadataFuzzer(data, size);
  cameraMetadataFuzzer.invokeCameraMetadataAPI();
  return 0;
}
