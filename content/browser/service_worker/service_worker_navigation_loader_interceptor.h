// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_type.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace content {

struct NavigationRequestInfo;
class ServiceWorkerNavigationHandle;

// This class is a work in progress for https://crbug.com/824858. It is used
// only when NavigationLoaderOnUI is enabled.
//
// Handles navigations for service worker clients (for now just documents, not
// web workers). Lives on the UI thread.
//
// The corresponding legacy class is ServiceWorkerControlleeRequestHandler which
// lives on the IO thread. Currently, this class just delegates to the
// legacy class by posting tasks to it on the IO thread.
class ServiceWorkerNavigationLoaderInterceptor final
    : public NavigationLoaderInterceptor {
 public:
  ServiceWorkerNavigationLoaderInterceptor(
      const NavigationRequestInfo& request_info,
      ServiceWorkerNavigationHandle* handle);
  ~ServiceWorkerNavigationLoaderInterceptor() override;

  // NavigationLoaderInterceptor overrides:

  // This could get called multiple times during the lifetime in redirect
  // cases. (In fallback-to-network cases we basically forward the request
  // to the request to the next request handler)
  void MaybeCreateLoader(const network::ResourceRequest& tentative_request,
                         BrowserContext* browser_context,
                         ResourceContext* resource_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;
  // Returns params with the ControllerServiceWorkerPtr if we have found
  // a matching controller service worker for the |request| that is given
  // to MaybeCreateLoader(). Otherwise this returns base::nullopt.
  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams()
      override;

  // These are called back from the IO thread helper functions:
  void LoaderCallbackWrapper(
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      LoaderCallback loader_callback,
      SingleRequestURLLoaderFactory::RequestHandler handler_on_io);
  void FallbackCallbackWrapper(FallbackCallback fallback_callback,
                               bool reset_subresource_loader_params);

  base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> GetWeakPtr();

 private:
  // Given as a callback to NavigationURLLoaderImpl.
  void RequestHandlerWrapper(
      SingleRequestURLLoaderFactory::RequestHandler handler_on_io,
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderRequest request,
      network::mojom::URLLoaderClientPtr client);

  // |handle_| owns |this|.
  ServiceWorkerNavigationHandle* const handle_;

  const bool are_ancestors_secure_;
  const int frame_tree_node_id_;
  const ResourceType resource_type_;
  const bool skip_service_worker_;

  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  base::WeakPtrFactory<ServiceWorkerNavigationLoaderInterceptor> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationLoaderInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_INTERCEPTOR_H_
