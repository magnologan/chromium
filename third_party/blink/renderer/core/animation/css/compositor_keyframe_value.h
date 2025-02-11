// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COMPOSITOR_KEYFRAME_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COMPOSITOR_KEYFRAME_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT CompositorKeyframeValue
    : public GarbageCollectedFinalized<CompositorKeyframeValue> {
 public:
  virtual ~CompositorKeyframeValue() = default;

  bool IsDouble() const { return GetType() == Type::kDouble; }
  bool IsFilterOperations() const {
    return GetType() == Type::kFilterOperations;
  }
  bool IsTransform() const { return GetType() == Type::kTransform; }
  bool IsColor() const { return GetType() == Type::kColor; }

  virtual void Trace(Visitor*) {}

 protected:
  enum class Type {
    kDouble,
    kFilterOperations,
    kTransform,
    kColor,
  };

 private:
  virtual Type GetType() const = 0;
};

#define DEFINE_COMPOSITOR_KEYFRAME_VALUE_TYPE_CASTS(thisType, predicate) \
  DEFINE_TYPE_CASTS(thisType, CompositorKeyframeValue, value,            \
                    value->predicate, value.predicate)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_COMPOSITOR_KEYFRAME_VALUE_H_
