// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_DETECTOR_H_
#define CHROME_BROWSER_USB_WEB_USB_DETECTOR_H_

#include <map>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "url/gurl.h"

class WebUsbDetector : public device::mojom::UsbDeviceManagerClient {
 public:
  WebUsbDetector();
  ~WebUsbDetector() override;

  // Initializes the WebUsbDetector.
  void Initialize();

  void SetDeviceManagerForTesting(
      device::mojom::UsbDeviceManagerPtr fake_device_manager);
  void RemoveNotification(const std::string& id);

 private:
  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  void OnDeviceManagerConnectionError();
  bool IsDisplayingNotification(const GURL& url);

  std::map<std::string, GURL> open_notifications_by_id_;

  // Connection to |device_manager_instance_|.
  device::mojom::UsbDeviceManagerPtr device_manager_;
  mojo::AssociatedBinding<device::mojom::UsbDeviceManagerClient>
      client_binding_;

  base::WeakPtrFactory<WebUsbDetector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebUsbDetector);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_DETECTOR_H_
