// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/android_management_client.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

AndroidManagementClient::AndroidManagementClient(
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& account_id,
    identity::IdentityManager* identity_manager)
    : device_management_service_(device_management_service),
      url_loader_factory_(url_loader_factory),
      account_id_(account_id),
      identity_manager_(identity_manager),
      weak_ptr_factory_(this) {}

AndroidManagementClient::~AndroidManagementClient() {}

void AndroidManagementClient::StartCheckAndroidManagement(
    const StatusCallback& callback) {
  DCHECK(device_management_service_);
  DCHECK(callback_.is_null());

  callback_ = callback;
  RequestAccessToken();
}

void AndroidManagementClient::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Token request failed: " << error.ToString();
    DCHECK(!callback_.is_null());
    std::move(callback_).Run(Result::ERROR);
    return;
  }

  CheckAndroidManagement(token_info.token);
}

void AndroidManagementClient::RequestAccessToken() {
  DCHECK(!access_token_fetcher_);
  // The user must be signed in already.
  DCHECK(identity_manager_->HasAccountWithRefreshToken(account_id_));

  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);

  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id_, "android_management_client", scopes,
      base::BindOnce(&AndroidManagementClient::OnAccessTokenFetchComplete,
                     base::Unretained(this)),
      identity::AccessTokenFetcher::Mode::kImmediate);
}

void AndroidManagementClient::CheckAndroidManagement(
    const std::string& access_token) {
  std::unique_ptr<DMServerJobConfiguration> config = std::make_unique<
      DMServerJobConfiguration>(
      device_management_service_,
      DeviceManagementService::JobConfiguration::TYPE_ANDROID_MANAGEMENT_CHECK,
      /*client_id=*/base::GenerateGUID(),
      /*critical=*/false, DMAuth::NoAuth(), access_token, url_loader_factory_,
      base::BindOnce(&AndroidManagementClient::OnAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));

  config->request()->mutable_check_android_management_request();

  request_job_ = device_management_service_->CreateJob(std::move(config));
}

void AndroidManagementClient::OnAndroidManagementChecked(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  DCHECK(!callback_.is_null());
  if (status == DM_STATUS_SUCCESS &&
      !response.has_check_android_management_response()) {
    LOG(WARNING) << "Invalid check android management response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  Result result;
  switch (status) {
    case DM_STATUS_SUCCESS:
      result = Result::UNMANAGED;
      break;
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      result = Result::MANAGED;
      break;
    default:
      result = Result::ERROR;
  }

  request_job_.reset();
  std::move(callback_).Run(result);
}

std::ostream& operator<<(std::ostream& os,
                         AndroidManagementClient::Result result) {
  switch (result) {
    case AndroidManagementClient::Result::MANAGED:
      return os << "MANAGED";
    case AndroidManagementClient::Result::UNMANAGED:
      return os << "UNMANAGED";
    case AndroidManagementClient::Result::ERROR:
      return os << "ERROR";
  }
  NOTREACHED();
  return os;
}

}  // namespace policy
