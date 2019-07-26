#pragma once

#include "net/base/net_export.h"

#include <bnl/result.hpp>

#include <cstdint>

namespace net {

enum class error { ok = 0, io_pending = -1, failed = -2 };

class domain;
using code = bnl::status_code<domain>;

class domain : public bnl::status_code_domain {
 public:
  using value_type = error;

  constexpr domain() noexcept : status_code_domain(0x5c9e37a99f3fc4e4) {}

  domain(const domain&) = default;
  domain(domain&&) = default;
  domain& operator=(const domain&) = default;
  domain& operator=(domain&&) = default;
  ~domain() = default;

  static inline constexpr const domain& get();

  string_ref name() const noexcept final;

  bool _do_failure(const bnl::status_code<void>& sc) const noexcept final;

  bool _do_equivalent(const bnl::status_code<void>& first,
                      const bnl::status_code<void>& second) const
      noexcept final;

  bnl::generic_code _generic_code(const bnl::status_code<void>& sc) const
      noexcept final;

  string_ref _do_message(const bnl::status_code<void>& sc) const noexcept final;
};

constexpr domain instance;

inline constexpr const domain& domain::get() {
  return instance;
}

code make_status_code(error error);

}  // namespace net
