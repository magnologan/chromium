// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/browser/content_index/content_index_database.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_index_context.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// Owned by the Storage Partition. Components that want to query or modify the
// Content Index database should hold a reference to this.
class CONTENT_EXPORT ContentIndexContextImpl
    : public ContentIndexContext,
      public base::RefCountedThreadSafe<ContentIndexContextImpl,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  ContentIndexContextImpl(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  void Shutdown();

  ContentIndexDatabase& database();

  // ContentIndexContent implementation.
  void GetIcon(int64_t service_worker_registration_id,
               const std::string& description_id,
               base::OnceCallback<void(SkBitmap)> icon_callback) override;
  void GetAllEntries(GetAllEntriesCallback callback) override;
  void GetEntry(int64_t service_worker_registration_id,
                const std::string& description_id,
                GetEntryCallback callback) override;
  void OnUserDeletedItem(int64_t service_worker_registration_id,
                         const url::Origin& origin,
                         const std::string& description_id) override;

 private:
  friend class base::DeleteHelper<ContentIndexContextImpl>;
  friend class base::RefCountedThreadSafe<ContentIndexContextImpl,
                                          BrowserThread::DeleteOnIOThread>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  ~ContentIndexContextImpl() override;

  // Called after a user-initiated delete.
  void DidDeleteItem(int64_t service_worker_registration_id,
                     const url::Origin& origin,
                     const std::string& description_id,
                     blink::mojom::ContentIndexError error);

  void StartActiveWorkerForDispatch(
      const std::string& description_id,
      blink::ServiceWorkerStatusCode service_worker_status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DeliverMessageToWorker(
      scoped_refptr<ServiceWorkerVersion> service_worker,
      scoped_refptr<ServiceWorkerRegistration> registration,
      const std::string& description_id,
      blink::ServiceWorkerStatusCode start_worker_status);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  ContentIndexDatabase content_index_database_;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_IMPL_H_
