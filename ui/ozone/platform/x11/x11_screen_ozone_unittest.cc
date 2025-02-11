// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::_;

namespace ui {

namespace {

constexpr gfx::Rect kPrimaryDisplayBounds(0, 0, 800, 600);

ACTION_P(StoreWidget, widget_ptr) {
  if (widget_ptr)
    *widget_ptr = arg0;
}

int64_t NextDisplayId() {
  static int64_t next_id = 0;
  return next_id++;
}

}  // namespace

class X11ScreenOzoneTest : public testing::Test {
 public:
  X11ScreenOzoneTest()
      : task_env_(std::make_unique<base::test::ScopedTaskEnvironment>(
            base::test::ScopedTaskEnvironment::MainThreadType::UI)) {}
  ~X11ScreenOzoneTest() override = default;

  void SetUp() override {
    XDisplay* display = gfx::GetXDisplay();
    event_source_ = std::make_unique<X11EventSourceLibevent>(display);
    window_manager_ = std::make_unique<X11WindowManagerOzone>();
    primary_display_ = std::make_unique<display::Display>(
        NextDisplayId(), kPrimaryDisplayBounds);
    screen_.reset(new X11ScreenOzone(window_manager_.get(), false));
    screen_->AddDisplay(*primary_display_, true);
  }

 protected:
  X11ScreenOzone* screen() const { return screen_.get(); }
  const display::Display& primary_display() const { return *primary_display_; }

  std::unique_ptr<display::Display> CreateDisplay(gfx::Rect bounds) const {
    return std::make_unique<display::Display>(NextDisplayId(), bounds);
  }

  void AddDisplayForTest(const display::Display& display) {
    screen_->AddDisplay(display, false);
  }

  void RemoveDisplayForTest(const display::Display& display) {
    screen_->RemoveDisplay(display);
  }

  std::unique_ptr<X11WindowOzone> CreatePlatformWindow(
      MockPlatformWindowDelegate* delegate,
      const gfx::Rect& bounds,
      gfx::AcceleratedWidget* widget = nullptr) {
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_))
        .WillOnce(StoreWidget(widget));
    PlatformWindowInitProperties init_params(bounds);
    return std::make_unique<X11WindowOzone>(delegate, init_params,
                                            window_manager_.get());
  }

 private:
  std::unique_ptr<X11WindowManagerOzone> window_manager_;
  std::unique_ptr<display::Display> primary_display_;
  std::unique_ptr<X11ScreenOzone> screen_;
  std::unique_ptr<X11EventSourceLibevent> event_source_;
  std::unique_ptr<base::test::ScopedTaskEnvironment> task_env_;

  DISALLOW_COPY_AND_ASSIGN(X11ScreenOzoneTest);
};

// This test case ensures that PlatformScreen correctly provides the display
// list as they are added/removed.
TEST_F(X11ScreenOzoneTest, AddRemoveListDisplays) {
  // Initially only primary display is expected to be in place
  EXPECT_EQ(1u, screen()->GetAllDisplays().size());

  auto display_2 = CreateDisplay(gfx::Rect(800, 0, 1280, 720));
  AddDisplayForTest(*display_2);
  EXPECT_EQ(2u, screen()->GetAllDisplays().size());

  auto display_3 = CreateDisplay(gfx::Rect(0, 720, 800, 600));
  AddDisplayForTest(*display_3);
  EXPECT_EQ(3u, screen()->GetAllDisplays().size());

  RemoveDisplayForTest(*display_3);
  EXPECT_EQ(2u, screen()->GetAllDisplays().size());
  RemoveDisplayForTest(*display_2);
  EXPECT_EQ(1u, screen()->GetAllDisplays().size());
  RemoveDisplayForTest(primary_display());
  EXPECT_EQ(0u, screen()->GetAllDisplays().size());
}

// This test case exercises GetDisplayForAcceleratedWidget when simple cases
// for platform windows in a single-display setup.
TEST_F(X11ScreenOzoneTest, GetDisplayForWidgetSingleDisplay) {
  auto primary = primary_display();
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(100, 100, 400, 300);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget);
  EXPECT_EQ(primary, screen()->GetDisplayForAcceleratedWidget(widget));
  EXPECT_EQ(primary, screen()->GetDisplayForAcceleratedWidget(
                         gfx::kNullAcceleratedWidget));

  MockPlatformWindowDelegate delegate_1;
  gfx::AcceleratedWidget widget_1;
  constexpr gfx::Rect bounds_1(kPrimaryDisplayBounds.width() + 100,
                               kPrimaryDisplayBounds.height() + 100, 200, 200);
  auto window_1 = CreatePlatformWindow(&delegate_1, bounds_1, &widget_1);
  EXPECT_EQ(primary, screen()->GetDisplayForAcceleratedWidget(widget_1));
}

// This test case exercises GetDisplayForAcceleratedWidget when simple cases
// for platform windows in a 2 side-by-side displays setup.
TEST_F(X11ScreenOzoneTest, GetDisplayForWidgetTwoDisplays) {
  auto display_2 =
      CreateDisplay(gfx::Rect(kPrimaryDisplayBounds.width(), 0, 1280, 720));
  AddDisplayForTest(*display_2);

  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(kPrimaryDisplayBounds.width() + 10, 100, 400, 300);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget);
  EXPECT_EQ(*display_2, screen()->GetDisplayForAcceleratedWidget(widget));

  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(1);
  window->SetBounds(
      gfx::Rect(kPrimaryDisplayBounds.width() - 250, 0, 400, 300));
  EXPECT_EQ(primary_display(),
            screen()->GetDisplayForAcceleratedWidget(widget));
}

// This test case exercises GetDisplayNearestPoint function simulating 2
// side-by-side displays setup.
TEST_F(X11ScreenOzoneTest, GetDisplayNearestPointTwoDisplays) {
  auto display_2 =
      CreateDisplay(gfx::Rect(kPrimaryDisplayBounds.width(), 0, 1280, 720));
  AddDisplayForTest(*display_2);

  EXPECT_EQ(primary_display(),
            screen()->GetDisplayNearestPoint(gfx::Point(10, 10)));
  EXPECT_EQ(primary_display(),
            screen()->GetDisplayNearestPoint(gfx::Point(790, 100)));
  EXPECT_EQ(*display_2, screen()->GetDisplayNearestPoint(gfx::Point(1000, 10)));
  EXPECT_EQ(*display_2,
            screen()->GetDisplayNearestPoint(gfx::Point(10000, 10000)));
}

// This test case exercises GetDisplayMatching function with both single and
// side-by-side display setup
TEST_F(X11ScreenOzoneTest, GetDisplayMatchingMultiple) {
  auto primary = primary_display();
  EXPECT_EQ(primary, screen()->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(primary,
            screen()->GetDisplayMatching(gfx::Rect(1000, 600, 100, 100)));

  auto second =
      CreateDisplay(gfx::Rect(kPrimaryDisplayBounds.width(), 0, 1280, 720));
  AddDisplayForTest(*second);
  EXPECT_EQ(primary, screen()->GetDisplayMatching(gfx::Rect(50, 50, 100, 100)));
  EXPECT_EQ(*second,
            screen()->GetDisplayMatching(gfx::Rect(1000, 100, 100, 100)));
  EXPECT_EQ(*second,
            screen()->GetDisplayMatching(gfx::Rect(1000, 600, 100, 100)));

  // Check rectangle overlapping 2 displays
  EXPECT_EQ(primary, screen()->GetDisplayMatching(gfx::Rect(740, 0, 100, 100)));
  EXPECT_EQ(*second,
            screen()->GetDisplayMatching(gfx::Rect(760, 100, 100, 100)));
}

}  // namespace ui
