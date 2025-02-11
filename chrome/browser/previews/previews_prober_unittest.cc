// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_prober.h"

#include <cmath>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kTestUrl("https://test.com");
const base::TimeDelta kCacheRevalidateAfter(base::TimeDelta::FromDays(1));
}  // namespace

class TestDelegate : public PreviewsProber::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() = default;

  bool ShouldSendNextProbe() override { return should_send_next_probe_; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::ResourceResponseHead& head,
                         std::unique_ptr<std::string> body) override {
    return net_error == net::OK &&
           head.headers->response_code() == net::HTTP_OK;
  }

  void set_should_send_next_probe(bool should_send_next_probe) {
    should_send_next_probe_ = should_send_next_probe;
  }

 private:
  bool should_send_next_probe_ = true;
};

class TestPreviewsProber : public PreviewsProber {
 public:
  TestPreviewsProber(
      PreviewsProber::Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      const PreviewsProber::ClientName name,
      const GURL& url,
      const HttpMethod http_method,
      const net::HttpRequestHeaders headers,
      const RetryPolicy& retry_policy,
      const TimeoutPolicy& timeout_policy,
      const size_t max_cache_entries,
      base::TimeDelta revalidate_cache_after,
      const base::TickClock* tick_clock,
      const base::Clock* clock)
      : PreviewsProber(delegate,
                       url_loader_factory,
                       pref_service,
                       name,
                       url,
                       http_method,
                       headers,
                       retry_policy,
                       timeout_policy,
                       max_cache_entries,
                       revalidate_cache_after,
                       tick_clock,
                       clock) {}
};

class PreviewsProberTest : public testing::Test {
 public:
  PreviewsProberTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        test_delegate_(),
        test_prefs_() {}

  void SetUp() override {
    PreviewsProber::RegisterProfilePrefs(test_prefs_.registry());
  }

  std::unique_ptr<PreviewsProber> NewProber() {
    return NewProberWithPolicies(PreviewsProber::RetryPolicy(),
                                 PreviewsProber::TimeoutPolicy());
  }

  std::unique_ptr<PreviewsProber> NewProberWithRetryPolicy(
      const PreviewsProber::RetryPolicy& retry_policy) {
    return NewProberWithPolicies(retry_policy, PreviewsProber::TimeoutPolicy());
  }

  std::unique_ptr<PreviewsProber> NewProberWithPolicies(
      const PreviewsProber::RetryPolicy& retry_policy,
      const PreviewsProber::TimeoutPolicy& timeout_policy) {
    return NewProberWithPoliciesAndDelegate(&test_delegate_, retry_policy,
                                            timeout_policy);
  }

  std::unique_ptr<PreviewsProber> NewProberWithPoliciesAndDelegate(
      PreviewsProber::Delegate* delegate,
      const PreviewsProber::RetryPolicy& retry_policy,
      const PreviewsProber::TimeoutPolicy& timeout_policy) {
    net::HttpRequestHeaders headers;
    headers.SetHeader("X-Testing", "Hello world");
    std::unique_ptr<TestPreviewsProber> prober =
        std::make_unique<TestPreviewsProber>(
            delegate, test_shared_loader_factory_, &test_prefs_,
            PreviewsProber::ClientName::kLitepages, kTestUrl,
            PreviewsProber::HttpMethod::kGet, headers, retry_policy,
            timeout_policy, 1, kCacheRevalidateAfter,
            thread_bundle_.GetMockTickClock(), thread_bundle_.GetMockClock());
    prober->SetOnCompleteCallback(base::BindRepeating(
        &PreviewsProberTest::OnProbeComplete, base::Unretained(this)));
    return prober;
  }

  void RunUntilIdle() { thread_bundle_.RunUntilIdle(); }

  void FastForward(base::TimeDelta delta) {
    thread_bundle_.FastForwardBy(delta);
  }

  void MakeResponseAndWait(net::HttpStatusCode http_status,
                           net::Error net_error) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    ASSERT_EQ(request->request.url.host(), kTestUrl.host());
    ASSERT_EQ(request->request.url.scheme(), kTestUrl.scheme());

    network::ResourceResponseHead head =
        network::CreateResourceResponseHead(http_status);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, head, "content",
                                         status);
    RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  void VerifyNoRequests() {
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  }

  void VerifyRequest(bool expect_random_guid = false) {
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

    std::string testing_header;
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    request->request.headers.GetHeader("X-Testing", &testing_header);

    EXPECT_EQ(testing_header, "Hello world");
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags, net::LOAD_DISABLE_CACHE);
    EXPECT_FALSE(request->request.allow_credentials);
    if (expect_random_guid) {
      EXPECT_NE(request->request.url, kTestUrl);
      EXPECT_TRUE(request->request.url.query().find("guid=") !=
                  std::string::npos);
      EXPECT_EQ(request->request.url.query().length(),
                5U /* len("guid=") */ + 36U /* len(hex guid with hyphens) */);
      // We don't check for the randomness of successive GUIDs on the assumption
      // base::GenerateGUID() is always correct.
    } else {
      EXPECT_EQ(request->request.url, kTestUrl);
    }
  }

  void OnProbeComplete(bool success) { callback_result_ = success; }

  base::Optional<bool> callback_result() { return callback_result_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestDelegate test_delegate_;
  TestingPrefServiceSimple test_prefs_;
  base::Optional<bool> callback_result_;
};

TEST_F(PreviewsProberTest, OK) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.Prober.NumAttemptsBeforeSuccess.Litepages", 1, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.ResponseCode.Litepages",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::OK), 1);
}

TEST_F(PreviewsProberTest, OK_Callback) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  EXPECT_TRUE(callback_result().has_value());
  EXPECT_TRUE(callback_result().value());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.ResponseCode.Litepages",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::OK), 1);
}

TEST_F(PreviewsProberTest, MultipleStart) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // Calling |SendNowIfInactive| many times should result in only one url
  // request, which is verified in |VerifyRequest|.
  prober->SendNowIfInactive(false);
  prober->SendNowIfInactive(false);
  prober->SendNowIfInactive(false);
  VerifyRequest();
}

TEST_F(PreviewsProberTest, NetworkChangeStartsProber) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  RunUntilIdle();

  EXPECT_TRUE(prober->is_active());
}

TEST_F(PreviewsProberTest, NetworkConnectionShardsCache) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // The different type of cellular networks shouldn't make a difference.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  RunUntilIdle();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_2G);
  RunUntilIdle();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());

  // Switching to WIFI does make a difference.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  RunUntilIdle();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
}

TEST_F(PreviewsProberTest, CacheMaxSize) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  FastForward(base::TimeDelta::FromSeconds(1));

  // Change the connection type and report a new probe result.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  RunUntilIdle();

  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());

  FastForward(base::TimeDelta::FromSeconds(1));

  // Then, flip back to the original connection type. The old probe status
  // should not be persisted since the max cache size for testing is 1.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
}

TEST_F(PreviewsProberTest, CacheAutoRevalidation) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Fast forward until just before revalidation time.
  FastForward(kCacheRevalidateAfter - base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Fast forward the rest of the way and check the prober is active again.
  FastForward(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());
}

TEST_F(PreviewsProberTest, PersistentCache) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Create a new prober instance and verify the cached probe result is used.
  prober = NewProber();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Fast forward past the cache revalidation and check that the revalidation
  // time was also persisted.
  FastForward(kCacheRevalidateAfter);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.ResponseCode.Litepages",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::OK), 1);
}

#if defined(OS_ANDROID)
TEST_F(PreviewsProberTest, StartInForeground) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  prober->SendNowIfInactive(true);
  EXPECT_TRUE(prober->is_active());
}

TEST_F(PreviewsProberTest, DoesntCallSendInForegroundIfInactive) {
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  EXPECT_FALSE(prober->is_active());
}
#endif

TEST_F(PreviewsProberTest, NetError) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 4);
  histogram_tester.ExpectTotalCount("Previews.Prober.ResponseCode.Litepages",
                                    0);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 4);
}

TEST_F(PreviewsProberTest, NetError_Callback) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  EXPECT_TRUE(callback_result().has_value());
  EXPECT_FALSE(callback_result().value());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 4);
  histogram_tester.ExpectTotalCount("Previews.Prober.ResponseCode.Litepages",
                                    0);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 4);
}

TEST_F(PreviewsProberTest, HttpError) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 4);
  histogram_tester.ExpectUniqueSample("Previews.Prober.ResponseCode.Litepages",
                                      net::HTTP_NOT_FOUND, 4);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::OK), 4);
}

TEST_F(PreviewsProberTest, TimeUntilSuccess) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  FastForward(base::TimeDelta::FromMilliseconds(11000));

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectTotalCount(
      "Previews.Prober.TimeUntilFailure.Litepages", 0);
  histogram_tester.ExpectUniqueSample(
      "Previews.Prober.TimeUntilSuccess.Litepages", 11000, 1);
}

TEST_F(PreviewsProberTest, TimeUntilFailure) {
  base::HistogramTester histogram_tester;

  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  FastForward(base::TimeDelta::FromMilliseconds(11000));

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectTotalCount(
      "Previews.Prober.TimeUntilSuccess.Litepages", 0);
  histogram_tester.ExpectUniqueSample(
      "Previews.Prober.TimeUntilFailure.Litepages", 11000, 1);
}

TEST_F(PreviewsProberTest, RandomGUID) {
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.use_random_urls = true;
  retry_policy.max_retries = 0;

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest(true /* expect_random_guid */);

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(PreviewsProberTest, RetryLinear) {
  base::HistogramTester histogram_tester;
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Second retry should be another 1000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 3);
  histogram_tester.ExpectTotalCount("Previews.Prober.ResponseCode.Litepages",
                                    0);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 3);
  histogram_tester.ExpectTotalCount(
      "Previews.Prober.NumAttemptsBeforeSuccess.Litepages", 0);
}

TEST_F(PreviewsProberTest, RetryThenSucceed) {
  base::HistogramTester histogram_tester;
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Second retry should be another 1000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectBucketCount("Previews.Prober.DidSucceed.Litepages",
                                     false, 2);
  histogram_tester.ExpectBucketCount("Previews.Prober.DidSucceed.Litepages",
                                     true, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.Prober.NumAttemptsBeforeSuccess.Litepages", 3, 1);
  histogram_tester.ExpectUniqueSample("Previews.Prober.ResponseCode.Litepages",
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectBucketCount("Previews.Prober.NetError.Litepages",
                                     std::abs(net::ERR_FAILED), 2);
  histogram_tester.ExpectBucketCount("Previews.Prober.NetError.Litepages",
                                     std::abs(net::OK), 1);
  histogram_tester.ExpectTotalCount("Previews.Prober.NetError.Litepages", 3);
}

TEST_F(PreviewsProberTest, RetryExponential) {
  base::HistogramTester histogram_tester;
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kExponential;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Second retry should be another 2000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(1999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 3);
  histogram_tester.ExpectTotalCount("Previews.Prober.ResponseCode.Litepages",
                                    0);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 3);
}

TEST_F(PreviewsProberTest, TimeoutLinear) {
  base::HistogramTester histogram_tester;
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 1;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(10);

  PreviewsProber::TimeoutPolicy timeout_policy;
  timeout_policy.backoff = PreviewsProber::Backoff::kLinear;
  timeout_policy.base_timeout = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithPolicies(retry_policy, timeout_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // First attempt.
  prober->SendNowIfInactive(false);
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Fast forward to the start of the next attempt.
  FastForward(base::TimeDelta::FromMilliseconds(10));

  // Second attempt should have the same timeout.
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 2);
  histogram_tester.ExpectTotalCount("Previews.Prober.ResponseCode.Litepages",
                                    0);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::ERR_TIMED_OUT), 2);
}

TEST_F(PreviewsProberTest, TimeoutExponential) {
  base::HistogramTester histogram_tester;
  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 1;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(10);

  PreviewsProber::TimeoutPolicy timeout_policy;
  timeout_policy.backoff = PreviewsProber::Backoff::kExponential;
  timeout_policy.base_timeout = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober =
      NewProberWithPolicies(retry_policy, timeout_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // First attempt.
  prober->SendNowIfInactive(false);
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Fast forward to the start of the next attempt.
  FastForward(base::TimeDelta::FromMilliseconds(10));

  // Second attempt should have a 2s timeout.
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.DidSucceed.Litepages",
                                      false, 2);
  histogram_tester.ExpectTotalCount("Previews.Prober.ResponseCode.Litepages",
                                    0);
  histogram_tester.ExpectUniqueSample("Previews.Prober.NetError.Litepages",
                                      std::abs(net::ERR_TIMED_OUT), 2);
}

TEST_F(PreviewsProberTest, DelegateStopsFirstProbe) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.set_should_send_next_probe(false);

  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober = NewProberWithPoliciesAndDelegate(
      &delegate, retry_policy, PreviewsProber::TimeoutPolicy());
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());
  VerifyNoRequests();

  histogram_tester.ExpectTotalCount("Previews.Prober.DidSucceed.Litepages", 0);
}

TEST_F(PreviewsProberTest, DelegateStopsRetries) {
  TestDelegate delegate;

  PreviewsProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = PreviewsProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<PreviewsProber> prober = NewProberWithPoliciesAndDelegate(
      &delegate, retry_policy, PreviewsProber::TimeoutPolicy());
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  delegate.set_should_send_next_probe(false);
  FastForward(base::TimeDelta::FromMilliseconds(1));

  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
  VerifyNoRequests();
}

TEST_F(PreviewsProberTest, CacheEntryAge) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<PreviewsProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample("Previews.Prober.CacheEntryAge.Litepages",
                                      0, 1);

  FastForward(base::TimeDelta::FromHours(24));
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());

  histogram_tester.ExpectBucketCount("Previews.Prober.CacheEntryAge.Litepages",
                                     0, 1);
  histogram_tester.ExpectBucketCount("Previews.Prober.CacheEntryAge.Litepages",
                                     24, 1);
  histogram_tester.ExpectTotalCount("Previews.Prober.CacheEntryAge.Litepages",
                                    2);
}
