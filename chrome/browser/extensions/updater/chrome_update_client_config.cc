// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_update_client_config.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/extensions/updater/extension_update_client_command_line_config_policy.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/extension_prefs.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {

namespace {

using FactoryCallback = ChromeUpdateClientConfig::FactoryCallback;

// static
static FactoryCallback& GetFactoryCallback() {
  static base::NoDestructor<FactoryCallback> factory;
  return *factory;
}

class ExtensionActivityDataService final
    : public update_client::ActivityDataService {
 public:
  explicit ExtensionActivityDataService(ExtensionPrefs* extension_prefs);
  ~ExtensionActivityDataService() override {}

  // update_client::ActivityDataService:
  bool GetActiveBit(const std::string& id) const override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;
  void ClearActiveBit(const std::string& id) override;

 private:
  // This member is not owned by this class, it's owned by a profile keyed
  // service.
  ExtensionPrefs* extension_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActivityDataService);
};

// Calculates the value to use for the ping days parameter.
int CalculatePingDays(const base::Time& last_ping_day) {
  return last_ping_day.is_null()
             ? update_client::kDaysFirstTime
             : std::max((base::Time::Now() - last_ping_day).InDays(), 0);
}

ExtensionActivityDataService::ExtensionActivityDataService(
    ExtensionPrefs* extension_prefs)
    : extension_prefs_(extension_prefs) {
  DCHECK(extension_prefs_);
}

bool ExtensionActivityDataService::GetActiveBit(const std::string& id) const {
  return extension_prefs_->GetActiveBit(id);
}

int ExtensionActivityDataService::GetDaysSinceLastActive(
    const std::string& id) const {
  return CalculatePingDays(extension_prefs_->LastActivePingDay(id));
}

int ExtensionActivityDataService::GetDaysSinceLastRollCall(
    const std::string& id) const {
  return CalculatePingDays(extension_prefs_->LastPingDay(id));
}

void ExtensionActivityDataService::ClearActiveBit(const std::string& id) {
  extension_prefs_->SetActiveBit(id, false);
}

}  // namespace

// For privacy reasons, requires encryption of the component updater
// communication with the update backend.
ChromeUpdateClientConfig::ChromeUpdateClientConfig(
    content::BrowserContext* context)
    : context_(context),
      impl_(ExtensionUpdateClientCommandLineConfigPolicy(
                base::CommandLine::ForCurrentProcess()),
            /*require_encryption=*/true),
      pref_service_(ExtensionPrefs::Get(context)->pref_service()),
      activity_data_service_(std::make_unique<ExtensionActivityDataService>(
          ExtensionPrefs::Get(context))) {
  DCHECK(pref_service_);
}

int ChromeUpdateClientConfig::InitialDelay() const {
  return impl_.InitialDelay();
}

int ChromeUpdateClientConfig::NextCheckDelay() const {
  return impl_.NextCheckDelay();
}

int ChromeUpdateClientConfig::OnDemandDelay() const {
  return impl_.OnDemandDelay();
}

int ChromeUpdateClientConfig::UpdateDelay() const {
  return impl_.UpdateDelay();
}

std::vector<GURL> ChromeUpdateClientConfig::UpdateUrl() const {
  return impl_.UpdateUrl();
}

std::vector<GURL> ChromeUpdateClientConfig::PingUrl() const {
  return impl_.PingUrl();
}

std::string ChromeUpdateClientConfig::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CRX);
}

base::Version ChromeUpdateClientConfig::GetBrowserVersion() const {
  return impl_.GetBrowserVersion();
}

std::string ChromeUpdateClientConfig::GetChannel() const {
  return chrome::GetChannelName();
}

std::string ChromeUpdateClientConfig::GetBrand() const {
  std::string brand;
  google_brand::GetBrand(&brand);
  return brand;
}

std::string ChromeUpdateClientConfig::GetLang() const {
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeUpdateClientConfig::GetOSLongName() const {
  return impl_.GetOSLongName();
}

base::flat_map<std::string, std::string>
ChromeUpdateClientConfig::ExtraRequestParams() const {
  return impl_.ExtraRequestParams();
}

std::string ChromeUpdateClientConfig::GetDownloadPreference() const {
  return std::string();
}

scoped_refptr<update_client::NetworkFetcherFactory>
ChromeUpdateClientConfig::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ =
        base::MakeRefCounted<update_client::NetworkFetcherChromiumFactory>(
            content::BrowserContext::GetDefaultStoragePartition(context_)
                ->GetURLLoaderFactoryForBrowserProcess());
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
ChromeUpdateClientConfig::GetUnzipperFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!unzip_factory_) {
    unzip_factory_ = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
        content::GetSystemConnector()->Clone());
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
ChromeUpdateClientConfig::GetPatcherFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!patch_factory_) {
    patch_factory_ = base::MakeRefCounted<update_client::PatchChromiumFactory>(
        content::GetSystemConnector()->Clone());
  }
  return patch_factory_;
}

bool ChromeUpdateClientConfig::EnabledDeltas() const {
  return impl_.EnabledDeltas();
}

bool ChromeUpdateClientConfig::EnabledComponentUpdates() const {
  return impl_.EnabledComponentUpdates();
}

bool ChromeUpdateClientConfig::EnabledBackgroundDownloader() const {
  return impl_.EnabledBackgroundDownloader();
}

bool ChromeUpdateClientConfig::EnabledCupSigning() const {
  return impl_.EnabledCupSigning();
}

PrefService* ChromeUpdateClientConfig::GetPrefService() const {
  return pref_service_;
}

update_client::ActivityDataService*
ChromeUpdateClientConfig::GetActivityDataService() const {
  return activity_data_service_.get();
}

bool ChromeUpdateClientConfig::IsPerUserInstall() const {
  return component_updater::IsPerUserInstall();
}

std::vector<uint8_t> ChromeUpdateClientConfig::GetRunActionKeyHash() const {
  return impl_.GetRunActionKeyHash();
}

std::string ChromeUpdateClientConfig::GetAppGuid() const {
  return impl_.GetAppGuid();
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
ChromeUpdateClientConfig::GetProtocolHandlerFactory() const {
  return impl_.GetProtocolHandlerFactory();
}

update_client::RecoveryCRXElevator
ChromeUpdateClientConfig::GetRecoveryCRXElevator() const {
  return impl_.GetRecoveryCRXElevator();
}

ChromeUpdateClientConfig::~ChromeUpdateClientConfig() {}

// static
scoped_refptr<ChromeUpdateClientConfig> ChromeUpdateClientConfig::Create(
    content::BrowserContext* context) {
  FactoryCallback& factory = GetFactoryCallback();
  return factory.is_null() ? scoped_refptr<ChromeUpdateClientConfig>(
                                 new ChromeUpdateClientConfig(context))
                           : factory.Run(context);
}

// static
void ChromeUpdateClientConfig::SetChromeUpdateClientConfigFactoryForTesting(
    FactoryCallback factory) {
  DCHECK(!factory.is_null());
  GetFactoryCallback() = factory;
}

}  // namespace extensions
