// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MANAGER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MANAGER_MAC_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/apps/app_shim/mach_bootstrap_acceptor.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace apps {
class ExtensionAppShimHandler;
}

namespace base {
class FilePath;
}

namespace test {
class AppShimHostManagerTestApi;
}

// The AppShimHostManager receives connections from app shims on a Mach
// bootstrap namespace entry (mach_acceptor_) and creates a helper object
// to manage the connection.
class AppShimHostManager : public apps::MachBootstrapAcceptor::Delegate,
                           public base::RefCountedThreadSafe<
                               AppShimHostManager,
                               content::BrowserThread::DeleteOnUIThread> {
 public:
  AppShimHostManager();

  // Init passes this AppShimHostManager to PostTask which requires it to have
  // a non-zero refcount. Therefore, Init cannot be called in the constructor
  // since the refcount is zero at that point.
  void Init();

  apps::ExtensionAppShimHandler* extension_app_shim_handler() {
    return extension_app_shim_handler_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<AppShimHostManager>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AppShimHostManager>;
  friend class test::AppShimHostManagerTestApi;
  virtual ~AppShimHostManager();

  // MachBootstrapAcceptor::Delegate:
  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         base::ProcessId peer_pid) override;

  // The |acceptor_| must be created on a thread which allows blocking I/O.
  void InitOnBackgroundThread();

  base::FilePath directory_in_tmp_;

  std::unique_ptr<apps::MachBootstrapAcceptor> mach_acceptor_;

  std::unique_ptr<apps::ExtensionAppShimHandler> extension_app_shim_handler_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManager);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MANAGER_MAC_H_
