// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/webui_client.h"

#include <utility>

#include "base/logging.h"

namespace notifications {

WebUIClient::WebUIClient() = default;

WebUIClient::~WebUIClient() = default;

void WebUIClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  std::move(callback).Run(std::move(notification_data));
}

void WebUIClient::OnSchedulerInitialized(bool success,
                                         std::set<std::string> guids) {
  NOTIMPLEMENTED();
}

void WebUIClient::OnUserAction(UserActionType action_type,
                               base::Optional<ButtonClickInfo> button_info) {
  NOTIMPLEMENTED();
}

}  // namespace notifications
