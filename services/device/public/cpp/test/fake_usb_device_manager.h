// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_MANAGER_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace device {

class MockUsbMojoDevice;

// This class implements a fake USB device manager which will only be used in
// tests for device::mojom::UsbDeviceManager's users.
class FakeUsbDeviceManager : public mojom::UsbDeviceManager {
 public:
  using DeviceMap =
      std::unordered_map<std::string, scoped_refptr<FakeUsbDeviceInfo>>;

  FakeUsbDeviceManager();
  ~FakeUsbDeviceManager() override;

  void AddBinding(mojom::UsbDeviceManagerRequest request);

  // Create a device and add it to added_devices_.
  template <typename... Args>
  mojom::UsbDeviceInfoPtr CreateAndAddDevice(Args&&... args) {
    scoped_refptr<FakeUsbDeviceInfo> device =
        new FakeUsbDeviceInfo(std::forward<Args>(args)...);
    return AddDevice(device);
  }

  mojom::UsbDeviceInfoPtr AddDevice(scoped_refptr<FakeUsbDeviceInfo> device);

  void RemoveDevice(const std::string& guid);

  void RemoveDevice(scoped_refptr<FakeUsbDeviceInfo> device);

  bool SetMockForDevice(const std::string& guid,
                        MockUsbMojoDevice* mock_device);

  bool IsBound() { return !bindings_.empty(); }

  void CloseAllBindings() { bindings_.CloseAllBindings(); }

  void RemoveAllDevices();

 protected:
  DeviceMap& devices() { return devices_; }

 private:
  // mojom::UsbDeviceManager implementation:
  void EnumerateDevicesAndSetClient(
      mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
      EnumerateDevicesAndSetClientCallback callback) override;
  void GetDevices(mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override;
  void GetDevice(const std::string& guid,
                 mojom::UsbDeviceRequest device_request,
                 mojom::UsbDeviceClientPtr device_client) override;

#if defined(OS_ANDROID)
  void RefreshDeviceInfo(const std::string& guid,
                         RefreshDeviceInfoCallback callback) override;
#endif

#if defined(OS_CHROMEOS)
  void CheckAccess(const std::string& guid,
                   CheckAccessCallback callback) override;

  void OpenFileDescriptor(const std::string& guid,
                          OpenFileDescriptorCallback callback) override;
#endif  // defined(OS_CHROMEOS)

  void SetClient(
      mojom::UsbDeviceManagerClientAssociatedPtrInfo client) override;

  mojo::BindingSet<mojom::UsbDeviceManager> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::UsbDeviceManagerClient> clients_;

  DeviceMap devices_;

  base::WeakPtrFactory<FakeUsbDeviceManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeUsbDeviceManager);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_MANAGER_H_
