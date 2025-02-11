// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PROXY_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PROXY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

class ServiceWorkerContextWrapper;

// This class is used to take messages from BackgroundSyncManager on the IO
// thread and pass them on BackgroundSyncController on the UI thread through
// its Core class which lives on the UI thread.
// This is owned by the BackgroundSyncManager.
class CONTENT_EXPORT BackgroundSyncProxy {
 public:
  explicit BackgroundSyncProxy(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~BackgroundSyncProxy();

  void ScheduleBrowserWakeUp(blink::mojom::BackgroundSyncType sync_type);

 private:
  // Constructed on the IO thread, lives and dies on the UI thread.
  class Core;

  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;
  base::WeakPtr<Core> ui_core_weak_ptr_;
  base::WeakPtrFactory<BackgroundSyncProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_PROXY_H_
