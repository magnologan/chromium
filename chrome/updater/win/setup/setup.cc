// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/util.h"

namespace updater {

namespace {

constexpr char kBuildGenWinDir[] = "gen\\chrome\\updater\\win";

const base::char16* kUpdaterFiles[] = {
    L"updater.exe",
#if defined(COMPONENT_BUILD)
    L"base.dll",    L"boringssl.dll", L"crcrypto.dll",      L"icuuc.dll",
    L"libc++.dll",  L"prefs.dll",     L"protobuf_lite.dll", L"updater.exe",
    L"url_lib.dll", L"zlib.dll",
#endif
};

}  // namespace

int Setup() {
  VLOG(1) << __func__;

  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    LOG(ERROR) << "GetTempDir failed.";
    return -1;
  }
  base::FilePath product_dir;
  if (!GetProductDirectory(&product_dir)) {
    LOG(ERROR) << "GetProductDirectory failed.";
    return -1;
  }
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(ERROR) << "PathService failed.";
    return -1;
  }

  installer::SelfCleaningTempDir backup_dir;
  if (!backup_dir.Initialize(temp_dir, L"updater-backup")) {
    LOG(ERROR) << "Failed to initialize the backup dir.";
    return -1;
  }

  const base::FilePath source_dir = exe_path.DirName();

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  for (const auto* file : kUpdaterFiles) {
    const base::FilePath target_path = product_dir.Append(file);
    const base::FilePath source_path = source_dir.Append(file);
    install_list->AddWorkItem(
        WorkItem::CreateCopyTreeWorkItem(source_path, target_path, temp_dir,
                                         WorkItem::ALWAYS, base::FilePath()));
  }
  install_list->AddWorkItem(WorkItem::CreateCopyTreeWorkItem(
      source_dir.AppendASCII(kBuildGenWinDir).AppendASCII(kUninstallScript),
      product_dir.AppendASCII(kUninstallScript), temp_dir, WorkItem::ALWAYS,
      base::FilePath()));

  if (!install_list->Do()) {
    LOG(ERROR) << "Install failed, rolling back...";
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete.";
    return -1;
  }

  VLOG(1) << "Setup succeeded.";
  return 0;
}

}  // namespace updater
