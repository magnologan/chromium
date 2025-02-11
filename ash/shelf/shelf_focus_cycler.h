// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_FOCUS_CYCLER_H_
#define ASH_SHELF_SHELF_FOCUS_CYCLER_H_

#include "base/macros.h"

namespace ash {
class Shelf;

// The containers that rely on ShelfFocusCycler to move focus outside of their
// view trees.
enum class SourceView {
  kShelfView = 0,
  kSystemTrayView,
};

// ShelfFocusCycler handles the special focus transitions from the Login UI,
// Shelf, and Status Tray.
class ShelfFocusCycler {
 public:
  explicit ShelfFocusCycler(Shelf* shelf);
  ~ShelfFocusCycler() = default;

  // Moves focus from one container to the next. |reverse| will move the focus
  // to the container to the left in LTR. RTL does not need to be accounted
  // for when calling this function.
  void FocusOut(bool reverse, SourceView source_view);

 private:
  // Owned by RootWindowController.
  Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfFocusCycler);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_FOCUS_CYCLER_H_
