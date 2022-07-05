#pragma once

#include "modules/video_capture/device_info_impl.h"
#include "rtp_encoding_parameters.h"
#ifdef __OBJC__
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

// TODO: add docs
class DeviceInfoMac : public webrtc::videocapturemodule::DeviceInfoImpl {
 public:
  DeviceInfoMac();
  ~DeviceInfoMac() override;
  uint32_t NumberOfDevices() override;
  int32_t GetDeviceName(uint32_t deviceNumber,
                        char* deviceNameUTF8,
                        uint32_t deviceNameLength,
                        char* deviceUniqueIdUTF8,
                        uint32_t deviceUniqueIdUTF8Length,
                        char* productUniqueIdUTF8 = 0,
                        uint32_t productUniqueIdUTF8Length = 0) override;
  int32_t CreateCapabilityMap(const char* deviceUniqueIdUTF8) override;
  int32_t DisplayCaptureSettingsDialogBox(const char* /*deviceUniqueIdUTF8*/,
                                          const char* /*dialogTitleUTF8*/,
                                          void* /*parentWindow*/,
                                          uint32_t /*positionX*/,
                                          uint32_t /*positionY*/) override;
  int32_t FillCapabilities(int fd) RTC_EXCLUSIVE_LOCKS_REQUIRED(_apiLock);
  int32_t Init() override;

 protected:
  AVCaptureDevice* device;
};
#endif

std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo>
create_device_info_mac();
