// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_fetcher.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace optimization_guide {

HintsFetcher::HintsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL optimization_guide_service_url)
    : optimization_guide_service_url_(net::AppendOrReplaceQueryParameter(
          optimization_guide_service_url,
          "key",
          features::GetOptimizationGuideServiceAPIKey())) {
  url_loader_factory_ = std::move(url_loader_factory);
  CHECK(optimization_guide_service_url_.SchemeIs(url::kHttpsScheme));
  CHECK(features::IsHintsFetchingEnabled());
}

HintsFetcher::~HintsFetcher() {}

bool HintsFetcher::FetchOptimizationGuideServiceHints(
    const std::vector<std::string>& hosts,
    HintsFetchedCallback hints_fetched_callback) {
  SEQUENCE_CHECKER(sequence_checker_);

  if (url_loader_)
    return false;

  get_hints_request_ = std::make_unique<proto::GetHintsRequest>();

  // Add all the optimizations supported by the current version of Chrome,
  // regardless of whether the session is in holdback for any of them.
  get_hints_request_->add_supported_optimizations(proto::NOSCRIPT);
  get_hints_request_->add_supported_optimizations(proto::RESOURCE_LOADING);
  get_hints_request_->add_supported_optimizations(proto::DEFER_ALL_SCRIPT);
  // TODO(crbug/969558): Figure out a way to either have a registration call
  // for clients to specify their supported optimization types or have a static
  // assert on the last OptimizationType.

  get_hints_request_->set_context(proto::CONTEXT_BATCH_UPDATE);

  for (const auto& host : hosts) {
    proto::HostInfo* host_info = get_hints_request_->add_hosts();
    host_info->set_host(host);
  }

  std::string serialized_request;
  get_hints_request_->SerializeToString(&serialized_request);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("hintsfetcher_gethintsrequest", R"(
        semantics {
          sender: "HintsFetcher"
          description:
            "Requests Hints from the Optimization Guide Service for use in "
            "providing data saving and pageload optimizations for Chrome."
          trigger:
            "Requested periodically if Data Saver is enabled and the browser "
            "has Hints that are older than a threshold set by "
            "the server."
          data: "A list of the user's most engaged websites."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Data Saver on Android via 'Data Saver' setting. "
            "Data Saver is not available on iOS."
          policy_exception_justification: "Not implemented."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = optimization_guide_service_url_;

  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_BYPASS_PROXY;
  resource_request->allow_credentials = false;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/x-protobuf");

  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", hosts.size());

  // |url_loader_| should not retry on 5xx errors since the server may already
  // be overloaded.  |url_loader_| should retry on network changes since the
  // network stack may receive the connection change event later than |this|.
  static const int kMaxRetries = 1;
  url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&HintsFetcher::OnURLLoadComplete, base::Unretained(this)));

  hints_fetched_callback_ = std::move(hints_fetched_callback);
  return true;
}

void HintsFetcher::HandleResponse(const std::string& get_hints_response_data,
                                  int net_status,
                                  int response_code) {
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  UMA_HISTOGRAM_ENUMERATION(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status",
      static_cast<net::HttpStatusCode>(response_code),
      net::HTTP_VERSION_NOT_SUPPORTED);
  // Net error codes are negative but histogram enums must be positive.
  base::UmaHistogramSparse(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode",
      -net_status);

  if (net_status == net::OK && response_code == net::HTTP_OK &&
      get_hints_response->ParseFromString(get_hints_response_data)) {
    std::move(hints_fetched_callback_).Run(std::move(get_hints_response));
  } else {
    std::move(hints_fetched_callback_).Run(base::nullopt);
  }
}

void HintsFetcher::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  HandleResponse(response_body ? *response_body : "", url_loader_->NetError(),
                 response_code);
  url_loader_.reset();
}

}  // namespace optimization_guide
