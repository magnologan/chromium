// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/noop_notification_schedule_service.h"

#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace notifications {

NoopNotificationScheduleService::NoopNotificationScheduleService() = default;

NoopNotificationScheduleService::~NoopNotificationScheduleService() = default;

void NoopNotificationScheduleService::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {}

void NoopNotificationScheduleService::DeleteNotifications(
    SchedulerClientType type) {}

void NoopNotificationScheduleService::GetImpressionDetail(
    SchedulerClientType,
    ImpressionDetail::ImpressionDetailCallback callback) {}

NotificationBackgroundTaskScheduler::Handler*
NoopNotificationScheduleService::GetBackgroundTaskSchedulerHandler() {
  return this;
}

UserActionHandler* NoopNotificationScheduleService::GetUserActionHandler() {
  return this;
}

void NoopNotificationScheduleService::OnStartTask(
    SchedulerTaskTime task_time,
    TaskFinishedCallback callback) {}

void NoopNotificationScheduleService::OnStopTask(SchedulerTaskTime task_time) {}

void NoopNotificationScheduleService::OnClick(SchedulerClientType type,
                                              const std::string& guid) {}

void NoopNotificationScheduleService::OnActionClick(
    SchedulerClientType type,
    const std::string& guid,
    ActionButtonType button_type) {}

void NoopNotificationScheduleService::OnDismiss(SchedulerClientType type,
                                                const std::string& guid) {}

}  // namespace notifications
