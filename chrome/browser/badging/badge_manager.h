// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace badging {

// The maximum value of badge contents before saturation occurs.
constexpr uint64_t kMaxBadgeContent = 99u;

// Determines the text to put on the badge based on some badge_content.
std::string GetBadgeString(base::Optional<uint64_t> badge_content);

// Maintains a record of badge contents and dispatches badge changes to a
// delegate.
class BadgeManager : public KeyedService, public blink::mojom::BadgeService {
 public:
  explicit BadgeManager(Profile* profile);
  ~BadgeManager() override;

  // Sets the delegate used for setting/clearing badges.
  void SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate);

  static void BindRequest(blink::mojom::BadgeServiceRequest request,
                          content::RenderFrameHost* frame);

  // Sets the badge for |app_id| to be |content|. Note: If content is set, it
  // must be non-zero.
  void UpdateAppBadge(const base::Optional<std::string>& app_id,
                      base::Optional<uint64_t> content);

  // Clears the badge for |app_id|.
  void ClearAppBadge(const base::Optional<std::string>& app_id);

 private:
  // The BindingContext of a mojo request. Allows mojo calls to be tied back to
  // the |RenderFrameHost| they belong to without trusting the renderer for that
  // information.
  struct BindingContext {
    BindingContext(int process_id, int frame_id)
        : process_id(process_id), frame_id(frame_id) {}
    int process_id;
    int frame_id;
  };

  // Notifies |delegate_| that a badge change was ignored.
  void BadgeChangeIgnored();

  // blink::mojom::BadgeService:
  // Note: These are private to stop them being called outside of mojo as they
  // require a mojo binding context.
  void SetInteger(uint64_t content) override;
  void SetFlag() override;
  void ClearBadge() override;

  // Examines |context| to determine which app, if any, should be badged.
  base::Optional<std::string> GetAppIdToBadge(const BindingContext& context);

  // All the mojo bindings for the BadgeManager. Keeps track of the
  // render_frame the binding is associated with, so as to not have to rely
  // on the renderer passing it in.
  mojo::BindingSet<blink::mojom::BadgeService, BindingContext> bindings_;

  // Delegate which handles actual setting and clearing of the badge.
  // Note: This is currently only set on Windows and MacOS.
  std::unique_ptr<BadgeManagerDelegate> delegate_;

  // Maps app id to badge contents.
  std::map<std::string, base::Optional<uint64_t>> badged_apps_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManager);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_H_
