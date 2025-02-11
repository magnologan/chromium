// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Icon sizes in DIP.
constexpr int kPrimaryIconSize = 20;
constexpr int kPrimaryIconBorderWidth = 8;

SkColor GetColorfromTheme() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  return native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
}

std::unique_ptr<views::ImageView> CreateIconView(const gfx::VectorIcon& icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(
      gfx::CreateVectorIcon(icon, kPrimaryIconSize, GetColorfromTheme()));
  icon_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPrimaryIconBorderWidth)));
  return icon_view;
}

std::unique_ptr<views::ImageView> CreateDeviceIcon(
    const sync_pb::SyncEnums::DeviceType device_type) {
  const gfx::VectorIcon* vector_icon;
  if (device_type == sync_pb::SyncEnums::TYPE_TABLET)
    vector_icon = &kTabletIcon;
  else
    vector_icon = &kHardwareSmartphoneIcon;
  return CreateIconView(*vector_icon);
}

}  // namespace

ClickToCallDialogView::ClickToCallDialogView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    ClickToCallSharingDialogController* controller)
    : LocationBarBubbleDelegateView(anchor_view, gfx::Point(), web_contents),
      controller_(controller),
      devices_(controller_->GetSyncedDevices()),
      apps_(controller_->GetApps()),
      send_failed_(controller->send_failed()) {}

ClickToCallDialogView::~ClickToCallDialogView() = default;

void ClickToCallDialogView::Hide() {
  CloseBubble();
}

int ClickToCallDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

std::unique_ptr<views::View> ClickToCallDialogView::CreateFootnoteView() {
  if (!devices_.empty() || apps_.empty() || send_failed_)
    return nullptr;

  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES),
      views::style::CONTEXT_LABEL, ChromeTextStyle::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  view->AddChildView(std::move(label));

  auto link = std::make_unique<views::Link>(l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_SYNC_HELP_TEXT));
  link->set_listener(this);
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  view->AddChildView(std::move(link));

  return view;
}

void ClickToCallDialogView::LinkClicked(views::Link* source, int event_flags) {
  controller_->OnHelpTextClicked();
}

void ClickToCallDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                   const gfx::Range& range,
                                                   int event_flags) {
  controller_->OnHelpTextClicked();
}

void ClickToCallDialogView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  dialog_buttons_.clear();

  auto* provider = ChromeLayoutProvider::Get();
  if (send_failed_) {
    set_margins(
        provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
    InitErrorView();
  } else if (devices_.empty() && apps_.empty()) {
    set_margins(
        provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
    InitEmptyView();
  } else {
    set_margins(
        gfx::Insets(provider->GetDistanceMetric(
                        views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                    0,
                    provider->GetDistanceMetric(
                        views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                    0));
    InitListView();
  }

  // TODO(yasmo): See if GetWidget can be not null:
  if (GetWidget())
    SizeToContents();
}

void ClickToCallDialogView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (!sender || sender->tag() < 0)
    return;
  size_t index = static_cast<size_t>(sender->tag());
  DCHECK(index < devices_.size() + apps_.size());

  if (index < devices_.size()) {
    LogClickToCallSelectedDeviceIndex(kSharingClickToCallUiDialog, index);
    controller_->OnDeviceChosen(devices_[index]);
    CloseBubble();
    return;
  }

  index -= devices_.size();

  if (index < apps_.size()) {
    LogClickToCallSelectedAppIndex(kSharingClickToCallUiDialog, index);
    controller_->OnAppChosen(apps_[index]);
    CloseBubble();
  }
}

void ClickToCallDialogView::InitListView() {
  int tag = 0;
  // Devices:
  LogClickToCallDevicesToShow(devices_.size());
  for (const auto& device : devices_) {
    auto dialog_button = std::make_unique<HoverButton>(
        this, CreateDeviceIcon(device.device_type()),
        device.human_readable_name(), /* subtitle= */ base::string16());
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }

  // Apps:
  LogClickToCallAppsToShow(apps_.size());
  for (const auto& app : apps_) {
    auto dialog_button =
        std::make_unique<HoverButton>(this, CreateIconView(app.icon), app.name,
                                      /* subtitle= */ base::string16());
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }
}

void ClickToCallDialogView::InitEmptyView() {
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES),
      views::style::CONTEXT_LABEL, ChromeTextStyle::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));

  auto link = std::make_unique<views::Link>(l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_SYNC_HELP_TEXT));
  link->set_listener(this);
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(link));
}

void ClickToCallDialogView::InitErrorView() {
  const base::string16 link = l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TROUBLESHOOT_LINK);
  size_t offset;
  const base::string16 text = l10n_util::GetStringFUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_FAILED_MESSAGE, link, &offset);
  auto label = std::make_unique<views::StyledLabel>(text, this);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  label->AddStyleRange(gfx::Range(offset, offset + link.length()), link_style);

  AddChildView(std::move(label));
}

gfx::Size ClickToCallDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool ClickToCallDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 ClickToCallDialogView::GetWindowTitle() const {
  if (send_failed_) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_FAILED_TO_SEND);
  } else if (apps_.empty() && devices_.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_NO_DEVICES);
  } else {
    return controller_->GetTitle();
  }
}

void ClickToCallDialogView::WindowClosing() {
  controller_->OnDialogClosed();
}
