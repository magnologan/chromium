// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_win.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/win/windows_version.h"
#include "chrome/browser/ui/webui/version_util_win.h"
#include "content/public/browser/web_ui.h"

VersionHandlerWindows::VersionHandlerWindows() : weak_factory_(this) {}

VersionHandlerWindows::~VersionHandlerWindows() {}

void VersionHandlerWindows::HandleRequestVersionInfo(
    const base::ListValue* args) {
  // Start the asynchronous load of the versions.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&version_utils::win::GetFullWindowsVersion),
      base::BindOnce(&VersionHandlerWindows::OnVersion,
                     weak_factory_.GetWeakPtr()));

  // Parent class takes care of the rest.
  VersionHandler::HandleRequestVersionInfo(args);
}

void VersionHandlerWindows::OnVersion(const std::string& version) {
  base::Value arg(version);
  CallJavascriptFunction("returnOsVersion", arg);
}

// static
std::string VersionHandlerWindows::GetFullWindowsVersionForTesting() {
  return version_utils::win::GetFullWindowsVersion();
}
