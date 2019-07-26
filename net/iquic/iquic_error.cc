#include "net/iquic/iquic_error.h"

namespace net {

domain::string_ref domain::name() const noexcept {
  return string_ref("chromium-net");
}

bool domain::_do_failure(const bnl::status_code<void>& sc) const noexcept {
  return static_cast<const code&>(sc).value() != error::ok;
}

bool domain::_do_equivalent(const bnl::status_code<void>& first,
                            const bnl::status_code<void>& second) const
    noexcept {
  if (second.domain() == *this) {
    return static_cast<const code&>(first).value() ==
           static_cast<const code&>(second).value();
  }

  return false;
}

bnl::generic_code domain::_generic_code(const bnl::status_code<void>& sc) const
    noexcept {
  (void)sc;
  return bnl::errc::unknown;
}

domain::string_ref domain::_do_message(const bnl::status_code<void>& sc) const
    noexcept {
  switch (static_cast<const code&>(sc).value()) {
    case error::ok:
      return string_ref("ok");
    case error::io_pending:
      return string_ref("io pending");
    case error::failed:
      return string_ref("failed");
  }

  return string_ref("unknown");
}

code make_status_code(error error) {
  return code(bnl::in_place, error);
}

}  // namespace net