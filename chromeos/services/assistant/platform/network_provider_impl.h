// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_NETWORK_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_NETWORK_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "libassistant/shared/public/platform_net.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class COMPONENT_EXPORT(ASSISTANT_SERVICE) NetworkProviderImpl
    : public assistant_client::NetworkProvider,
      public network_config::mojom::CrosNetworkConfigObserver {
 public:
  NetworkProviderImpl(service_manager::Connector* connector);
  ~NetworkProviderImpl() override;

  // assistant_client::NetworkProvider:
  ConnectionStatus GetConnectionStatus() override;
  assistant_client::MdnsResponder* GetMdnsResponder() override;

  network_config::mojom::CrosNetworkConfigObserverPtr BindAndGetPtr();

  // network_config::mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks)
      override;
  void OnNetworkStateListChanged() override {}
  void OnDeviceStateListChanged() override {}

 private:
  void Init(service_manager::Connector* connector);

  void BindCrosNetworkConfig(service_manager::Connector* connector);
  void AddAndFireCrosNetworkConfigObserver();

  ConnectionStatus connection_status_;
  mojo::Binding<network_config::mojom::CrosNetworkConfigObserver> binding_;
  network_config::mojom::CrosNetworkConfigPtr cros_network_config_ptr_;

  DISALLOW_COPY_AND_ASSIGN(NetworkProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_NETWORK_PROVIDER_IMPL_H_
