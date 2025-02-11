// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_painter.h"

#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_flags.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace {
// Placeholder colors and alphas. TODO(crbug.com/933384): get rid of
// these and make colors configurable, with same defaults as existing
// ink drops.
constexpr SkColor kInstallableInkDropBaseColor = SK_ColorBLACK;
constexpr float kInstallableInkDropHighlightedOpacity = 0.08;
constexpr float kInstallableInkDropActivatedOpacity = 0.16;
}  // namespace

namespace views {

InstallableInkDropPainter::State::State() = default;
InstallableInkDropPainter::State::~State() = default;

gfx::Size InstallableInkDropPainter::GetMinimumSize() const {
  return gfx::Size();
}

void InstallableInkDropPainter::Paint(gfx::Canvas* canvas,
                                      const gfx::Size& size) {
  TRACE_EVENT0("views", "InstallableInkDropPainter::Paint");

  DCHECK_GE(state_->flood_fill_progress, 0.0f);
  DCHECK_LE(state_->flood_fill_progress, 1.0f);
  DCHECK_GE(state_->highlighted_ratio, 0.0f);
  DCHECK_LE(state_->highlighted_ratio, 1.0f);

  // If fully filled, there is no need to draw the highlight, and we can draw
  // the activated color more efficiently as a rectangle.
  if (state_->flood_fill_progress == 1.0f) {
    canvas->FillRect(
        gfx::Rect(size),
        SkColorSetA(kInstallableInkDropBaseColor,
                    kInstallableInkDropActivatedOpacity * SK_AlphaOPAQUE));
    return;
  }

  if (state_->highlighted_ratio > 0.0f) {
    canvas->FillRect(
        gfx::Rect(size),
        SkColorSetA(kInstallableInkDropBaseColor,
                    kInstallableInkDropHighlightedOpacity *
                        state_->highlighted_ratio * SK_AlphaOPAQUE));
  }

  if (state_->flood_fill_progress > 0.0f) {
    // We interpolate between a circle of radius 2 and a circle whose radius is
    // the diagonal of |size|.
    const float min_radius = 2.0f;
    const float max_radius =
        gfx::Vector2dF(size.width(), size.height()).Length();
    const float cur_radius = gfx::Tween::FloatValueBetween(
        state_->flood_fill_progress, min_radius, max_radius);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        SkColorSetA(kInstallableInkDropBaseColor,
                    kInstallableInkDropActivatedOpacity * SK_AlphaOPAQUE));
    canvas->DrawCircle(state_->flood_fill_center, cur_radius, flags);
  }
}

}  // namespace views
