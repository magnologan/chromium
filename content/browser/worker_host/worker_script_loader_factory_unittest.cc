// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_loader_factory.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kProcessId = 1;

}  // namespace

class WorkerScriptLoaderFactoryTest : public testing::Test {
 public:
  WorkerScriptLoaderFactoryTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~WorkerScriptLoaderFactoryTest() override = default;

  void SetUp() override {
    // Set up the service worker system.
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    ServiceWorkerContextCore* context = helper_->context();
    context->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    resource_context_getter_ =
        base::BindRepeating(&ServiceWorkerContextWrapper::resource_context,
                            helper_->context_wrapper());

    // Set up the network factory.
    network_loader_factory_instance_ =
        std::make_unique<FakeNetworkURLLoaderFactory>();
    network::mojom::URLLoaderFactoryPtrInfo factory;
    network_loader_factory_instance_->Clone(mojo::MakeRequest(&factory));
    auto info = std::make_unique<network::WrapperSharedURLLoaderFactoryInfo>(
        std::move(factory));
    network_loader_factory_ =
        network::SharedURLLoaderFactory::Create(std::move(info));

    // Set up a service worker host for the shared worker.
    service_worker_provider_info_ =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    service_worker_provider_host_ =
        ServiceWorkerProviderHost::PreCreateForWebWorker(
            helper_->context()->AsWeakPtr(), kProcessId,
            blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
            &service_worker_provider_info_);
  }

 protected:
  network::mojom::URLLoaderPtr CreateTestLoaderAndStart(
      const GURL& url,
      WorkerScriptLoaderFactory* factory,
      network::TestURLLoaderClient* client) {
    network::mojom::URLLoaderPtr loader;
    network::ResourceRequest resource_request;
    resource_request.url = url;
    resource_request.resource_type =
        static_cast<int>(ResourceType::kSharedWorker);
    factory->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request,
        client->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return loader;
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::unique_ptr<FakeNetworkURLLoaderFactory> network_loader_factory_instance_;
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  blink::mojom::ServiceWorkerProviderInfoForClientPtr
      service_worker_provider_info_;
  base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host_;

  WorkerScriptLoaderFactory::ResourceContextGetter resource_context_getter_;
};

TEST_F(WorkerScriptLoaderFactoryTest, ServiceWorkerProviderHost) {
  // Make the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      kProcessId, service_worker_provider_host_, nullptr /* appcache_host */,
      resource_context_getter_, network_loader_factory_);

  // Load the script.
  GURL url("https://www.example.com/worker.js");
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);

  // The provider host should be set up.
  EXPECT_TRUE(service_worker_provider_host_->is_response_committed());
  EXPECT_TRUE(service_worker_provider_host_->is_execution_ready());
  EXPECT_EQ(url, service_worker_provider_host_->url());
}

// Test a null service worker provider host. This typically only happens during
// shutdown or after a fatal error occurred in the service worker system.
TEST_F(WorkerScriptLoaderFactoryTest, NullServiceWorkerProviderHost) {
  // Make the factory with null provider host.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      kProcessId, nullptr /* service_worker_provider_host */,
      nullptr /* appcache_host */, resource_context_getter_,
      network_loader_factory_);

  // Load the script.
  GURL url("https://www.example.com/worker.js");
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

// Test a null resource context when the request starts. This happens when
// shutdown starts between the constructor and when CreateLoaderAndStart is
// invoked.
TEST_F(WorkerScriptLoaderFactoryTest, NullResourceContext) {
  // Make the factory.
  auto factory = std::make_unique<WorkerScriptLoaderFactory>(
      kProcessId, service_worker_provider_host_, nullptr /* appcache_host */,
      resource_context_getter_, network_loader_factory_);

  // Set a null resource context.
  helper_->context_wrapper()->InitializeResourceContext(nullptr);

  // Load the script.
  GURL url("https://www.example.com/worker.js");
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader =
      CreateTestLoaderAndStart(url, factory.get(), &client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// TODO(falken): Add a test for a shared worker that's controlled by a service
// worker.

}  // namespace content
