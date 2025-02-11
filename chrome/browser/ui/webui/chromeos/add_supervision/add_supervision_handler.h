// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebUI;
}  // namespace content

namespace identity {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace identity

class GoogleServiceAuthError;

namespace chromeos {

class AddSupervisionHandler
    : public add_supervision::mojom::AddSupervisionHandler {
 public:
  // Interface for Delegates for specific behavior of AddSupervisionHandler.
  class Delegate {
   public:
    // Implementing methods should override this to implement
    // the request to close the Add Supervision dialog and return
    // a boolean to indicate whether the dialog is closing.
    virtual bool CloseDialog() = 0;
  };

  // |delegate| is owned by the caller and its lifetime must outlive |this|.
  AddSupervisionHandler(
      add_supervision::mojom::AddSupervisionHandlerRequest request,
      content::WebUI* web_ui,
      Delegate* delegate);
  ~AddSupervisionHandler() override;

  // add_supervision::mojom::AddSupervisionHandler overrides:
  void RequestClose(RequestCloseCallback callback) override;
  void GetInstalledArcApps(GetInstalledArcAppsCallback callback) override;
  void GetOAuthToken(GetOAuthTokenCallback callback) override;
  void LogOut() override;
  void NotifySupervisionEnabled() override;

 private:
  void OnAccessTokenFetchComplete(GetOAuthTokenCallback callback,
                                  GoogleServiceAuthError error,
                                  identity::AccessTokenInfo access_token_info);

  // The AddSupervisionUI that this AddSupervisionHandler belongs to.
  content::WebUI* web_ui_;

  // Used to fetch OAuth2 access tokens.
  identity::IdentityManager* identity_manager_;
  std::unique_ptr<identity::AccessTokenFetcher> oauth2_access_token_fetcher_;

  mojo::Binding<add_supervision::mojom::AddSupervisionHandler> binding_;

  Delegate* delegate_;

  base::WeakPtrFactory<AddSupervisionHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_
