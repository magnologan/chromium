// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/confirm_password_change_handler.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

ConfirmPasswordChangeHandler::ConfirmPasswordChangeHandler() {
  auto* in_session_password_change_manager =
      g_browser_process->platform_part()->in_session_password_change_manager();
  CHECK(in_session_password_change_manager);
  in_session_password_change_manager->AddObserver(this);
}

ConfirmPasswordChangeHandler::~ConfirmPasswordChangeHandler() {
  auto* in_session_password_change_manager =
      g_browser_process->platform_part()->in_session_password_change_manager();
  CHECK(in_session_password_change_manager);
  in_session_password_change_manager->RemoveObserver(this);
}

void ConfirmPasswordChangeHandler::OnEvent(
    InSessionPasswordChangeManager::Event event) {
  if (event ==
      InSessionPasswordChangeManager::CRYPTOHOME_PASSWORD_CHANGE_FAILURE) {
    AllowJavascript();
    FireWebUIListener("incorrect-old-password");
  }
}

void ConfirmPasswordChangeHandler::HandleChangePassword(
    const base::ListValue* params) {
  const std::string old_password = params->GetList()[0].GetString();
  const std::string new_password = params->GetList()[1].GetString();
  auto* in_session_password_change_manager =
      g_browser_process->platform_part()->in_session_password_change_manager();
  CHECK(in_session_password_change_manager);
  in_session_password_change_manager->ChangePassword(old_password,
                                                     new_password);
}

void ConfirmPasswordChangeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "changePassword",
      base::BindRepeating(&ConfirmPasswordChangeHandler::HandleChangePassword,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
