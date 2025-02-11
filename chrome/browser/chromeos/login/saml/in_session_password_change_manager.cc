// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"

#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/saml/password_expiry_notification.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

InSessionPasswordChangeManager* g_test_instance = nullptr;

// Traits for running RecheckPasswordExpiryTask.
// Runs from the UI thread to show notification.
const base::TaskTraits kRecheckTaskTraits = {
    content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// A time delta of length one hour.
const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);

// A time delta of length one day.
const base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);

// A time delta with length of a half day.
const base::TimeDelta kHalfDay = base::TimeDelta::FromHours(12);

// A time delta with length zero.
const base::TimeDelta kZeroTime = base::TimeDelta();

// When the password will expire in |kUrgentWarningDays| or less, the
// UrgentPasswordExpiryNotification will be used - which is larger and actually
// a dialog (not a true notification) - instead of the normal notification.
const int kUrgentWarningDays = 3;

// Rounds to the nearest day - eg plus or minus 12 hours is zero days, 12 to 36
// hours is 1 day, -12 to -36 hours is -1 day, etc.
inline int RoundToDays(base::TimeDelta time_delta) {
  return (time_delta + kHalfDay).InDaysFloored();
}

}  // namespace

RecheckPasswordExpiryTask::RecheckPasswordExpiryTask() = default;

RecheckPasswordExpiryTask::~RecheckPasswordExpiryTask() = default;

void RecheckPasswordExpiryTask::Recheck() {
  CancelPendingRecheck();
  InSessionPasswordChangeManager::Get()->MaybeShowExpiryNotification();
}

void RecheckPasswordExpiryTask::RecheckAfter(base::TimeDelta delay) {
  CancelPendingRecheck();
  base::PostDelayedTaskWithTraits(
      FROM_HERE, kRecheckTaskTraits,
      base::BindOnce(&RecheckPasswordExpiryTask::Recheck,
                     weak_ptr_factory_.GetWeakPtr()),
      std::max(delay, kOneHour));
  // This always waits at least one hour before calling Recheck again - we don't
  // want some bug to cause this code to run every millisecond.
}

void RecheckPasswordExpiryTask::CancelPendingRecheck() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

// static
std::unique_ptr<InSessionPasswordChangeManager>
InSessionPasswordChangeManager::CreateIfEnabled(Profile* primary_profile) {
  if (base::FeatureList::IsEnabled(features::kInSessionPasswordChange) &&
      primary_profile->GetPrefs()->GetBoolean(
          prefs::kSamlInSessionPasswordChangeEnabled)) {
    std::unique_ptr<InSessionPasswordChangeManager> manager =
        std::make_unique<InSessionPasswordChangeManager>(primary_profile);
    manager->MaybeShowExpiryNotification();
    return manager;
  } else {
    // If the policy is disabled, clear the SAML password attributes.
    SamlPasswordAttributes::DeleteFromPrefs(primary_profile->GetPrefs());
    return nullptr;
  }
}

// static
bool InSessionPasswordChangeManager::IsInitialized() {
  return GetNullable();
}

// static
InSessionPasswordChangeManager* InSessionPasswordChangeManager::Get() {
  InSessionPasswordChangeManager* result = GetNullable();
  CHECK(result);
  return result;
}

InSessionPasswordChangeManager::InSessionPasswordChangeManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)),
      authenticator_(new ChromeCryptohomeAuthenticator(this)),
      urgent_warning_days_(kUrgentWarningDays) {
  DCHECK(primary_user_);

  // Add |this| as a SessionActivationObserver to see when the screen is locked.
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddSessionActivationObserverForAccountId(
        primary_user_->GetAccountId(), this);
  }
}

InSessionPasswordChangeManager::~InSessionPasswordChangeManager() {
  // Remove |this| as a SessionActivationObserver.
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddSessionActivationObserverForAccountId(
        primary_user_->GetAccountId(), this);
  }
}

void InSessionPasswordChangeManager::MaybeShowExpiryNotification() {
  // We are checking password expiry now, and this function will decide if we
  // want to check again in the future, so for now, make sure there are no other
  // pending tasks to check aggain.
  recheck_task_.CancelPendingRecheck();

  PrefService* prefs = primary_profile_->GetPrefs();
  if (!prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled)) {
    DismissExpiryNotification();
    return;
  }

  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (!attrs.has_expiration_time()) {
    DismissExpiryNotification();
    return;
  }

  // Calculate how many days until the password will expire.
  const base::TimeDelta time_until_expiry =
      attrs.expiration_time() - base::Time::Now();
  const int days_until_expiry = RoundToDays(time_until_expiry);
  const int advance_warning_days =
      prefs->GetInteger(prefs::kSamlPasswordExpirationAdvanceWarningDays);

  bool is_expired = time_until_expiry <= kZeroTime;
  // Show notification if a) expired, or b) advance_warning_days is set > 0 and
  // we are now within advance_warning_days of the expiry time.
  const bool show_notification =
      is_expired ||
      (advance_warning_days > 0 && days_until_expiry <= advance_warning_days);

  if (show_notification) {
    // Show as urgent if urgent_warning_days is set > 0 and we are now within
    // urgent_warning_days of the expiry time.
    const bool show_as_urgent =
        (urgent_warning_days_ > 0 && days_until_expiry <= urgent_warning_days_);
    if (show_as_urgent) {
      ShowUrgentExpiryNotification();
    } else {
      ShowStandardExpiryNotification(time_until_expiry);
    }

    // We check again whether to reshow / update the notification after one day:
    recheck_task_.RecheckAfter(kOneDay);

  } else {
    // We have not yet reached the advance warning threshold. Check again
    // once we have arrived at expiry_time minus advance_warning_days...
    base::TimeDelta recheck_delay =
        time_until_expiry - base::TimeDelta::FromDays(advance_warning_days);
    // But, wait an extra hour so that when this code is next run, it is clear
    // we are now inside advance_warning_days (and not right on the boundary).
    recheck_delay += kOneHour;
    recheck_task_.RecheckAfter(recheck_delay);
  }
}

void InSessionPasswordChangeManager::ShowStandardExpiryNotification(
    base::TimeDelta time_until_expiry) {
  // Show a notification, and reshow it each time the screen is unlocked.
  renotify_on_unlock_ = true;
  PasswordExpiryNotification::Show(primary_profile_, time_until_expiry);
  UrgentPasswordExpiryNotificationDialog::Dismiss();
}

void InSessionPasswordChangeManager::ShowUrgentExpiryNotification() {
  // Show a notification, and reshow it each time the screen is unlocked.
  renotify_on_unlock_ = true;
  UrgentPasswordExpiryNotificationDialog::Show();
  PasswordExpiryNotification::Dismiss(primary_profile_);
}

void InSessionPasswordChangeManager::DismissExpiryNotification() {
  UrgentPasswordExpiryNotificationDialog::Dismiss();
  PasswordExpiryNotification::Dismiss(primary_profile_);
}

void InSessionPasswordChangeManager::OnExpiryNotificationDismissedByUser() {
  // When a notification is dismissed, we then don't pop it up again each time
  // the user unlocks the screen.
  renotify_on_unlock_ = false;
}

void InSessionPasswordChangeManager::OnScreenUnlocked() {
  if (renotify_on_unlock_) {
    MaybeShowExpiryNotification();
  }
}

void InSessionPasswordChangeManager::StartInSessionPasswordChange() {
  NotifyObservers(START_SAML_IDP_PASSWORD_CHANGE);
  DismissExpiryNotification();
  PasswordChangeDialog::Show(primary_profile_);
}

void InSessionPasswordChangeManager::OnSamlPasswordChanged(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password) {
  NotifyObservers(START_SAML_IDP_PASSWORD_CHANGE);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), true);
  PasswordChangeDialog::Dismiss();

  const bool both_passwords_scraped =
      !scraped_old_password.empty() && !scraped_new_password.empty();
  if (both_passwords_scraped) {
    // Both passwords scraped so we try to change cryptohome password now.
    // Show the confirm dialog but initially showing a spinner. If the change
    // fails, then the dialog will hide the spinner and show a prompt.
    // If the change succeeds, the dialog and spinner will just disappear.
    ConfirmPasswordChangeDialog::Show(scraped_old_password,
                                      scraped_new_password,
                                      /*show_spinner_initially=*/true);
    ChangePassword(scraped_old_password, scraped_new_password);
  } else {
    // Failed to scrape passwords - prompt for passwords immediately.
    ConfirmPasswordChangeDialog::Show(scraped_old_password,
                                      scraped_new_password,
                                      /*show_spinner_initially=*/false);
  }
}

void InSessionPasswordChangeManager::ChangePassword(
    const std::string& old_password,
    const std::string& new_password) {
  NotifyObservers(START_CRYPTOHOME_PASSWORD_CHANGE);
  UserContext user_context(*primary_user_);
  user_context.SetKey(Key(new_password));
  authenticator_->MigrateKey(user_context, old_password);
}

void InSessionPasswordChangeManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void InSessionPasswordChangeManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void InSessionPasswordChangeManager::OnAuthFailure(const AuthFailure& error) {
  VLOG(1) << "Failed to change cryptohome password: " << error.GetErrorString();
  NotifyObservers(CRYPTOHOME_PASSWORD_CHANGE_FAILURE);
}

void InSessionPasswordChangeManager::OnAuthSuccess(
    const UserContext& user_context) {
  VLOG(3) << "Cryptohome password is changed.";
  NotifyObservers(CRYPTOHOME_PASSWORD_CHANGED);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      user_context.GetAccountId(), false);

  // Clear expiration time from prefs so that we don't keep nagging the user to
  // change password (until the SAML provider tells us a new expiration time).
  SamlPasswordAttributes loaded =
      SamlPasswordAttributes::LoadFromPrefs(primary_profile_->GetPrefs());
  SamlPasswordAttributes(
      /*modified_time=*/base::Time::Now(), /*expiration_time=*/base::Time(),
      loaded.password_change_url())
      .SaveToPrefs(primary_profile_->GetPrefs());

  DismissExpiryNotification();
  PasswordChangeDialog::Dismiss();
  ConfirmPasswordChangeDialog::Dismiss();
}

void InSessionPasswordChangeManager::OnSessionActivated(bool activated) {
  // Not needed.
}

void InSessionPasswordChangeManager::OnLockStateChanged(bool locked) {
  if (!locked) {
    OnScreenUnlocked();
  }
}

// static
InSessionPasswordChangeManager* InSessionPasswordChangeManager::GetNullable() {
  return g_test_instance ? g_test_instance
                         : g_browser_process->platform_part()
                               ->in_session_password_change_manager();
}

// static
void InSessionPasswordChangeManager::SetForTesting(
    InSessionPasswordChangeManager* instance) {
  CHECK(!g_test_instance);
  g_test_instance = instance;
}

// static
void InSessionPasswordChangeManager::ResetForTesting() {
  g_test_instance = nullptr;
}

void InSessionPasswordChangeManager::NotifyObservers(Event event) {
  for (auto& observer : observer_list_) {
    observer.OnEvent(event);
  }
}

}  // namespace chromeos
