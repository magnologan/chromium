// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/dice_header_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "content/public/common/url_loader_throttle.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace {

// A delegate to insert a user generated X-Chrome-Connected header
// to a specifict URL.
class HeaderModifyingThrottle : public content::URLLoaderThrottle {
 public:
  HeaderModifyingThrottle() = default;
  ~HeaderModifyingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    request->headers.SetHeader(signin::kChromeConnectedHeader, "User Data");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HeaderModifyingThrottle);
};

class ThrottleContentBrowserClient : public ChromeContentBrowserClient {
 public:
  explicit ThrottleContentBrowserClient(const GURL& watch_url)
      : watch_url_(watch_url) {}
  ~ThrottleContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<content::URLLoaderThrottle>>
  CreateURLLoaderThrottlesOnIO(
      const network::ResourceRequest& request,
      content::ResourceContext* resource_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<content::URLLoaderThrottle>> throttles;
    if (request.url == watch_url_)
      throttles.push_back(std::make_unique<HeaderModifyingThrottle>());
    return throttles;
  }
  std::vector<std::unique_ptr<content::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    return CreateURLLoaderThrottlesOnIO(request, nullptr, wc_getter,
                                        navigation_ui_data, frame_tree_node_id);
  }

 private:
  const GURL watch_url_;

  DISALLOW_COPY_AND_ASSIGN(ThrottleContentBrowserClient);
};

// Subclass of DiceManageAccountBrowserTest with Mirror enabled.
class MirrorBrowserTest : public InProcessBrowserTest {
 private:
  void SetUpOnMainThread() override {
    // The test makes requests to google.com and other domains which we want to
    // redirect to the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    google_util::IgnorePortNumbersForGoogleURLChecksForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  ScopedAccountConsistencyMirror scoped_mirror_;
};

// Verify the following items:
// 1- X-Chrome-Connected is appended on Google domains if account
//    consistency is enabled and access is secure.
// 2- The header is stripped in case a request is redirected from a Gooogle
//    domain to non-google domain.
// 3- The header is NOT stripped in case it is added directly by the page
//    and not because it was on a secure Google domain.
// This is a regression test for crbug.com/588492.
IN_PROC_BROWSER_TEST_F(MirrorBrowserTest, MirrorRequestHeader) {
  browser()->profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                              "user@gmail.com");
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesUserAccountId, "account_id");

  base::Lock lock;
  // Map from the path of the URLs that test server sees to the request header.
  // This is the path, and not URL, because the requests use different domains
  // which the mock HostResolver converts to 127.0.0.1.
  std::map<std::string, net::test_server::HttpRequest::HeaderMap> header_map;
  embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        base::AutoLock auto_lock(lock);
        header_map[request.GetURL().path()] = request.headers;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        base::AutoLock auto_lock(lock);
        header_map[request.GetURL().path()] = request.headers;
      }));
  ASSERT_TRUE(https_server.Start());

  base::FilePath root_http;
  base::PathService::Get(chrome::DIR_TEST_DATA, &root_http);
  root_http = root_http.AppendASCII("mirror_request_header");

  struct TestCase {
    GURL original_url;  // The URL from which the request begins.
    // The path to which navigation is redirected.
    std::string redirected_to_path;
    bool inject_header;  // Should X-Chrome-Connected header be injected to the
                         // original request.
    bool original_url_expects_header;       // Expectation: The header should be
                                            // visible in original URL.
    bool redirected_to_url_expects_header;  // Expectation: The header should be
                                            // visible in redirected URL.
  };

  std::vector<TestCase> all_tests;

  // Neither should have the header.
  // Note we need to replace the port of the redirect's URL.
  base::StringPairs replacement_text;
  replacement_text.push_back(std::make_pair(
      "{{PORT}}", base::NumberToString(embedded_test_server()->port())));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/mirror_request_header/http.www.google.com.html", replacement_text);
  all_tests.push_back(
      {embedded_test_server()->GetURL("www.google.com", replacement_path),
       "/simple.html", false, false, false});

  // First one adds the header and transfers it to the second.
  replacement_path = net::test_server::GetFilePathWithReplacements(
      "/mirror_request_header/http.www.header_adder.com.html",
      replacement_text);
  all_tests.push_back(
      {embedded_test_server()->GetURL("www.header_adder.com", replacement_path),
       "/simple.html", true, true, true});

  // First one should have the header, but not transfered to second one.
  replacement_text.clear();
  replacement_text.push_back(
      std::make_pair("{{PORT}}", base::NumberToString(https_server.port())));
  replacement_path = net::test_server::GetFilePathWithReplacements(
      "/mirror_request_header/https.www.google.com.html", replacement_text);
  all_tests.push_back({https_server.GetURL("www.google.com", replacement_path),
                       "/simple.html", false, true, false});

  for (const auto& test_case : all_tests) {
    SCOPED_TRACE(test_case.original_url);

    // If test case requires adding header for the first url add a throttle.
    ThrottleContentBrowserClient browser_client(test_case.original_url);
    content::ContentBrowserClient* old_browser_client = nullptr;
    if (test_case.inject_header)
      old_browser_client = content::SetBrowserClientForTesting(&browser_client);

    // Navigate to first url.
    ui_test_utils::NavigateToURL(browser(), test_case.original_url);

    if (test_case.inject_header)
      content::SetBrowserClientForTesting(old_browser_client);

    base::AutoLock auto_lock(lock);

    // Check if header exists and X-Chrome-Connected is correctly provided.
    ASSERT_EQ(1u, header_map.count(test_case.original_url.path()));
    if (test_case.original_url_expects_header) {
      ASSERT_TRUE(!!header_map[test_case.original_url.path()].count(
          signin::kChromeConnectedHeader));
    } else {
      ASSERT_FALSE(!!header_map[test_case.original_url.path()].count(
          signin::kChromeConnectedHeader));
    }

    ASSERT_EQ(1u, header_map.count(test_case.redirected_to_path));
    if (test_case.redirected_to_url_expects_header) {
      ASSERT_TRUE(!!header_map[test_case.redirected_to_path].count(
          signin::kChromeConnectedHeader));
    } else {
      ASSERT_FALSE(!!header_map[test_case.redirected_to_path].count(
          signin::kChromeConnectedHeader));
    }

    header_map.clear();
  }
}

}  // namespace
