// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kerberos/kerberos_ticket_expiry_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;

namespace chromeos {

namespace {

constexpr char kUser[] = "user@EXAMPLE.COM";

constexpr char kNotificationId[] = "kerberos.ticket-expiry-notification";

class KerberosTicketExpiryNotificationTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test");
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  void TearDown() override { display_service_tester_.reset(); }

  void OnNotificationClick(const std::string& principal_name) {
    notification_click_count_[principal_name]++;
  }

 protected:
  base::Optional<Notification> Notification() {
    return display_service_tester_->GetNotification(kNotificationId);
  }

  void Show() {
    kerberos_ticket_expiry_notification::Show(
        profile_, kUser,
        base::BindRepeating(
            &KerberosTicketExpiryNotificationTest::OnNotificationClick,
            base::Unretained(this)));
  }

  content::TestBrowserThreadBundle test_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  TestingProfile* profile_ = nullptr;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Counts how many times a notification for a given user was clicked.
  std::map<std::string, int> notification_click_count_;
};

}  // namespace

TEST_F(KerberosTicketExpiryNotificationTest, ShowClose) {
  Show();
  ASSERT_TRUE(Notification().has_value());

  // Don't check the exact text here, just check if the username is there.
  EXPECT_NE(std::string::npos,
            Notification()->message().find(base::ASCIIToUTF16(kUser)));

  kerberos_ticket_expiry_notification::Close(profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(KerberosTicketExpiryNotificationTest, Click) {
  Show();
  EXPECT_EQ(0, notification_click_count_[kUser]);
  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationId,
      base::nullopt /* action_index */, base::nullopt /* reply */);
  EXPECT_EQ(1, notification_click_count_[kUser]);
}

}  // namespace chromeos
