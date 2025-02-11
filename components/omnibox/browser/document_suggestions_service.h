// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace identity {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace identity

class GoogleServiceAuthError;
class TemplateURLService;

// A service to fetch suggestions from a remote endpoint given a URL.
class DocumentSuggestionsService : public KeyedService {
 public:
  // null may be passed for params, but no request will be issued.
  DocumentSuggestionsService(
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~DocumentSuggestionsService() override;

  using StartCallback = base::OnceCallback<void(
      std::unique_ptr<network::SimpleURLLoader> loader)>;

  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              std::unique_ptr<std::string> response_body)>;

  // Creates and starts a document suggestion request for |query|.
  // May obtain an OAuth2 token for the signed-in user.
  void CreateDocumentSuggestionsRequest(
      const base::string16& query,
      const TemplateURLService* template_url_service,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  // Advises the service to stop any process that creates a suggestion request.
  void StopCreatingDocumentSuggestionsRequest();

 private:
  // Called when an access token request completes (successfully or not).
  void AccessTokenAvailable(std::unique_ptr<network::ResourceRequest> request,
                            std::string request_body,
                            net::NetworkTrafficAnnotationTag traffic_annotation,
                            StartCallback start_callback,
                            CompletionCallback completion_callback,
                            GoogleServiceAuthError error,
                            identity::AccessTokenInfo access_token_info);

  // Activates a loader for |request|, wiring it up to |completion_callback|,
  // and calls |start_callback|. If |request_body| isn't empty, it will be
  // attached as upload bytes.
  void StartDownloadAndTransferLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  identity::IdentityManager* identity_manager_;

  // Helper for fetching OAuth2 access tokens. Non-null when we have a token
  // available, or while a token fetch is in progress.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(DocumentSuggestionsService);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_SUGGESTIONS_SERVICE_H_
