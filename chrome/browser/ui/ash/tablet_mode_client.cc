// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/tablet_mode_client.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/material_design/material_design_controller.h"

namespace {

TabletModeClient* g_tablet_mode_client_instance = nullptr;

}  // namespace

TabletModeClient::TabletModeClient() {
  DCHECK(!g_tablet_mode_client_instance);
  g_tablet_mode_client_instance = this;
}

TabletModeClient::~TabletModeClient() {
  DCHECK_EQ(this, g_tablet_mode_client_instance);
  g_tablet_mode_client_instance = nullptr;
  // The Ash Shell and TabletMode instance should have been destroyed by now.
  DCHECK(!ash::TabletMode::Get());
}

void TabletModeClient::Init() {
  ash::TabletMode::Get()->AddObserver(this);
  OnTabletModeToggled(ash::TabletMode::Get()->InTabletMode());
}

// static
TabletModeClient* TabletModeClient::Get() {
  return g_tablet_mode_client_instance;
}

void TabletModeClient::AddObserver(TabletModeClientObserver* observer) {
  observers_.AddObserver(observer);
}

void TabletModeClient::RemoveObserver(TabletModeClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabletModeClient::OnTabletModeToggled(bool enabled) {
  if (tablet_mode_enabled_ == enabled)
    return;

  tablet_mode_enabled_ = enabled;

  SetMobileLikeBehaviorEnabled(enabled);

  ui::MaterialDesignController::OnTabletModeToggled(enabled);
  for (auto& observer : observers_)
    observer.OnTabletModeToggled(enabled);
}

void TabletModeClient::OnTabletModeStarted() {
  OnTabletModeToggled(true);
}

void TabletModeClient::OnTabletModeEnded() {
  OnTabletModeToggled(false);
}

void TabletModeClient::OnTabletControllerDestroyed() {
  ash::TabletMode::Get()->RemoveObserver(this);
}

bool TabletModeClient::ShouldTrackBrowser(Browser* browser) {
  return tablet_mode_enabled_;
}

void TabletModeClient::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;

  // We limit the mobile-like behavior to webcontents in tabstrips since many
  // apps and extensions draw their own caption buttons and header frames. We
  // don't want those to shrink down and resize to fit the width of their
  // windows like webpages on mobile do. So this behavior is limited to webpages
  // in tabs and packaged apps.
  for (const auto& contents : change.GetInsert()->contents)
    contents.contents->NotifyPreferencesChanged();
}

void TabletModeClient::SetMobileLikeBehaviorEnabled(bool enabled) {
  // Toggling tablet mode on/off should trigger refreshing the WebKit
  // preferences, since in tablet mode, we enable certain mobile-like features
  // such as "double tap to zoom", "shrink page contents to fit", ... etc.
  // Do this only for webpages that belong to existing browsers as well as
  // future browsers and webcontents.
  if (enabled) {
    // On calling Init() of the |tab_strip_tracker_|, we will get a call to
    // TabInsertedAt() for all the existing webcontents, upon which we will
    // trigger a refresh of their WebKit preferences.
    tab_strip_tracker_ =
        std::make_unique<BrowserTabStripTracker>(this, this, nullptr);
    tab_strip_tracker_->Init();
  } else {
    // Manually trigger a refresh for the existing webcontents' preferences.
    for (Browser* browser : *BrowserList::GetInstance()) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();
      for (int i = 0; i < tab_strip_model->count(); ++i) {
        content::WebContents* web_contents =
            tab_strip_model->GetWebContentsAt(i);
        DCHECK(web_contents);

        web_contents->NotifyPreferencesChanged();

        // For a tab that is requesting its mobile version site (via
        // chrome::ToggleRequestTabletSite()), and is not originated from ARC
        // context, return to its normal version site when exiting tablet mode.
        content::NavigationController& controller =
            web_contents->GetController();
        content::NavigationEntry* entry = controller.GetLastCommittedEntry();
        if (entry && entry->GetIsOverridingUserAgent() &&
            !web_contents->GetUserData(
                arc::ArcWebContentsData::ArcWebContentsData::
                    kArcTransitionFlag)) {
          entry->SetIsOverridingUserAgent(false);
          controller.Reload(content::ReloadType::ORIGINAL_REQUEST_URL, true);
        }
      }
    }
    tab_strip_tracker_ = nullptr;
  }
}
