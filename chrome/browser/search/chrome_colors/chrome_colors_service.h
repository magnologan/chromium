// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_CHROME_COLORS_CHROME_COLORS_SERVICE_H_
#define CHROME_BROWSER_SEARCH_CHROME_COLORS_CHROME_COLORS_SERVICE_H_

#include "base/callback.h"
#include "chrome/browser/themes/theme_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"

class TestChromeColorsService;

namespace chrome_colors {

// Supports theme changes originating from the NTP customization menu. Users can
// apply a Chrome color or the default theme, which will then either be reverted
// or confirmed and made permanent. If third party themes are present, users
// will also have a choice to permanently uninstall it.
class ChromeColorsService : public KeyedService {
 public:
  explicit ChromeColorsService(Profile* profile);
  ~ChromeColorsService() override;

  // Applies a theme that can be reverted by saving the previous theme state and
  // the |tab| that changes are made from.
  void ApplyDefaultTheme(content::WebContents* tab);
  void ApplyAutogeneratedTheme(SkColor color, content::WebContents* tab);

  // Reverts to the previous theme state before first Apply* was used.
  void RevertThemeChanges();

  // Same as |RevertThemeChanges| but only reverts theme changes if they were
  // made from the same tab. Used for reverting changes from a closing NTP.
  void RevertThemeChangesForTab(content::WebContents* tab);

  // Confirms current theme changes. Since the theme is already installed by
  // Apply*, this only clears the previously saved state.
  void ConfirmThemeChanges();

 private:
  friend class ::TestChromeColorsService;

  // Saves the necessary state(revert callback and the current tab) for
  // performing theme change revert. Saves the state only if it is not set.
  void SaveThemeRevertState(content::WebContents* tab);

  // KeyedService implementation:
  void Shutdown() override;

  ThemeService* const theme_service_;

  // The first tab that used Apply* and hasn't Confirm/Revert the changes.
  content::WebContents* dialog_tab_ = nullptr;

  // Callback that will revert the theme to the state it was at the time of this
  // callback's creation.
  base::OnceClosure revert_theme_changes_;

  DISALLOW_COPY_AND_ASSIGN(ChromeColorsService);
};

}  // namespace chrome_colors

#endif  // CHROME_BROWSER_SEARCH_CHROME_COLORS_CHROME_COLORS_SERVICE_H_
