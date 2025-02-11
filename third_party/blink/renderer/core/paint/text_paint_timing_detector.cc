// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

#include <memory>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool LargeTextFirst(const base::WeakPtr<TextRecord>& a,
                    const base::WeakPtr<TextRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  return a->insertion_index_ < b->insertion_index_;
}

}  // namespace

TextPaintTimingDetector::TextPaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector)
    : records_manager_(frame_view, paint_timing_detector),
      frame_view_(frame_view) {}

void LargestTextPaintManager::PopulateTraceValue(
    TracedValue& value,
    const TextRecord& first_text_paint) {
  // TODO(crbug.com/976893): Remove DOMNodeId.
  value.SetInteger("DOMNodeId", static_cast<int>(first_text_paint.node_id));
  value.SetInteger("size", static_cast<int>(first_text_paint.first_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
}

void LargestTextPaintManager::ReportCandidateToTrace(
    const TextRecord& largest_text_record) {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_text_record);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestTextPaint::Candidate",
                                   largest_text_record.paint_time, "data",
                                   std::move(value), "frame",
                                   ToTraceValue(&frame_view_->GetFrame()));
}

void LargestTextPaintManager::ReportNoCandidateToTrace() {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++count_candidates_);
  value->SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("isOOPIF",
                    !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
  TRACE_EVENT2("loading", "LargestTextPaint::NoCandidate", "data",
               std::move(value), "frame",
               ToTraceValue(&frame_view_->GetFrame()));
}

void LargestTextPaintManager::UpdateCandidate() {
  base::WeakPtr<TextRecord> largest_text_record = FindLargestPaintCandidate();
  const base::TimeTicks time =
      largest_text_record ? largest_text_record->paint_time : base::TimeTicks();
  const uint64_t size =
      largest_text_record ? largest_text_record->first_size : 0;
  DCHECK(paint_timing_detector_);
  bool changed =
      paint_timing_detector_->NotifyIfChangedLargestTextPaint(time, size);
  if (!changed)
    return;

  if (!time.is_null()) {
    if (auto* lcp_calculator =
            paint_timing_detector_->GetLargestContentfulPaintCalculator())
      lcp_calculator->OnLargestTextUpdated(largest_text_record);
    ReportCandidateToTrace(*largest_text_record);
  } else {
    if (auto* lcp_calculator =
            paint_timing_detector_->GetLargestContentfulPaintCalculator())
      lcp_calculator->OnLargestTextUpdated(nullptr);
    ReportNoCandidateToTrace();
  }
}

void TextPaintTimingDetector::OnPaintFinished() {
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    if (records_manager_.GetLargestTextPaintManager())
      records_manager_.GetLargestTextPaintManager()->UpdateCandidate();
  }
  if (records_manager_.NeedMeausuringPaintTime()) {
    if (!awaiting_swap_promise_) {
      // |WrapCrossThreadWeakPersistent| guarantees that when |this| is killed,
      // the callback function will not be invoked.
      RegisterNotifySwapTime(
          CrossThreadBindOnce(&TextPaintTimingDetector::ReportSwapTime,
                              WrapCrossThreadWeakPersistent(this)));
    }
  }
}

void TextPaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  if (!is_recording_)
    return;
  if (records_manager_.IsKnownVisible(object)) {
    records_manager_.RemoveVisibleRecord(object);
    need_update_timing_at_frame_end_ = true;
  } else if (records_manager_.IsKnownInvisible(object)) {
    records_manager_.RemoveInvisibleRecord(object);
    need_update_timing_at_frame_end_ = true;
  }
}

void TextPaintTimingDetector::RegisterNotifySwapTime(
    ReportTimeCallback callback) {
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;
  frame.GetPage()->GetChromeClient().NotifySwapTime(frame, std::move(callback));
  awaiting_swap_promise_ = true;
}

void TextPaintTimingDetector::ReportSwapTime(WebWidgetClient::SwapResult result,
                                             base::TimeTicks timestamp) {
  if (!is_recording_)
    return;
  if (!records_manager_.HasTextElementTiming()) {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document && RuntimeEnabledFeatures::ElementTimingEnabled(document)) {
      LocalDOMWindow* window = document->domWindow();
      if (window) {
        records_manager_.SetTextElementTiming(
            &TextElementTiming::From(*window));
      }
    }
  }
  records_manager_.AssignPaintTimeToQueuedRecords(timestamp);
  if (records_manager_.GetLargestTextPaintManager())
    records_manager_.GetLargestTextPaintManager()->UpdateCandidate();
  awaiting_swap_promise_ = false;
}

bool TextPaintTimingDetector::ShouldWalkObject(
    const LayoutBoxModelObject& object) const {
  if (!is_recording_)
    return false;
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = object.GetNode();
  if (!node)
    return false;
  // If we have finished recording Largest Text Paint and the element is a
  // shadow element or has no elementtiming attribute, then we should not record
  // its text.
  if (!records_manager_.IsRecordingLargestTextPaint() &&
      !TextElementTiming::NeededForElementTiming(*node))
    return false;

  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(node);
  if (node_id == kInvalidDOMNodeId)
    return true;
  // This metric defines the size of a text block by its first size, so we
  // should not walk the object if it has been recorded.
  return !records_manager_.HasRecorded(object);
}

void TextPaintTimingDetector::RecordAggregatedText(
    const LayoutBoxModelObject& aggregator,
    const IntRect& aggregated_visual_rect,
    const PropertyTreeState& property_tree_state) {
  DCHECK(ShouldWalkObject(aggregator));

  // The caller should check this.
  DCHECK(!aggregated_visual_rect.IsEmpty());

  FloatRect mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          aggregated_visual_rect, property_tree_state);
  uint64_t aggregated_size = mapped_visual_rect.Size().Area();

  if (aggregated_size == 0) {
    records_manager_.RecordInvisibleObject(aggregator);
  } else {
    records_manager_.RecordVisibleObject(
        aggregator, aggregated_size,
        TextElementTiming::ComputeIntersectionRect(
            aggregator, aggregated_visual_rect, property_tree_state,
            frame_view_));
  }
}

void TextPaintTimingDetector::StopRecordEntries() {
  is_recording_ = false;
  records_manager_.CleanUp();
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  records_manager_.CleanUpLargestTextPaint();
}

void TextPaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(records_manager_);
  visitor->Trace(frame_view_);
}

LargestTextPaintManager::LargestTextPaintManager(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector)
    : size_ordered_set_(&LargeTextFirst),
      frame_view_(frame_view),
      paint_timing_detector_(paint_timing_detector) {}

void LargestTextPaintManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
  visitor->Trace(paint_timing_detector_);
}

void TextRecordsManager::RemoveVisibleRecord(const LayoutObject& object) {
  DCHECK(visible_objects_.Contains(&object));
  if (ltp_manager_) {
    ltp_manager_->RemoveVisibleRecord(
        visible_objects_.at(&object)->AsWeakPtr());
  }
  visible_objects_.erase(&object);
  // We don't need to remove elements in |texts_queued_for_paint_time_| and
  // |cached_largest_paint_candidate_| as they are weak ptr.
}

void TextRecordsManager::CleanUpLargestTextPaint() {
  ltp_manager_.reset();
}

void TextRecordsManager::RemoveInvisibleRecord(const LayoutObject& object) {
  DCHECK(invisible_objects_.Contains(&object));
  invisible_objects_.erase(&object);
}

void TextRecordsManager::AssignPaintTimeToQueuedRecords(
    const base::TimeTicks& timestamp) {
  // If texts_queued_for_paint_time_.size == 0, it means the array has been
  // consumed in a callback earlier than this one. That violates the assumption
  // that only one or zero callback will be called after one OnPaintFinished.
  DCHECK_GT(texts_queued_for_paint_time_.size(), 0UL);
  for (auto iterator = texts_queued_for_paint_time_.begin();
       iterator != texts_queued_for_paint_time_.end(); ++iterator) {
    // The record may have been removed between the callback registration and
    // invoking.
    base::WeakPtr<TextRecord>& record = *iterator;
    if (!record) {
      texts_queued_for_paint_time_.erase(iterator);
      continue;
    }
    DCHECK_EQ(record->paint_time, base::TimeTicks());
    record->paint_time = timestamp;
  }
  if (text_element_timing_)
    text_element_timing_->OnTextObjectsPainted(texts_queued_for_paint_time_);
  texts_queued_for_paint_time_.clear();
  if (ltp_manager_)
    ltp_manager_->SetCachedResultInvalidated(true);
}

void TextRecordsManager::RecordVisibleObject(
    const LayoutObject& object,
    const uint64_t& visual_size,
    const FloatRect& element_timing_rect) {
  DCHECK_GT(visual_size, 0u);

  Node* node = object.GetNode();
  DCHECK(node);
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);
  std::unique_ptr<TextRecord> record =
      std::make_unique<TextRecord>(node_id, visual_size, element_timing_rect);
  base::WeakPtr<TextRecord> record_weak_ptr = record->AsWeakPtr();
  if (ltp_manager_)
    ltp_manager_->InsertRecord(record_weak_ptr);

  QueueToMeasurePaintTime(record_weak_ptr);
  visible_objects_.insert(&object, std::move(record));
}

base::WeakPtr<TextRecord> LargestTextPaintManager::FindLargestPaintCandidate() {
  if (!is_result_invalidated_ && cached_largest_paint_candidate_)
    return cached_largest_paint_candidate_;
  base::WeakPtr<TextRecord> new_largest_paint_candidate = nullptr;
  for (const auto& text_record : size_ordered_set_) {
    DCHECK(text_record);
    if (text_record->paint_time.is_null())
      continue;
    new_largest_paint_candidate = text_record;
    break;
  }
  cached_largest_paint_candidate_ = new_largest_paint_candidate;
  is_result_invalidated_ = false;
  return new_largest_paint_candidate;
}

TextRecordsManager::TextRecordsManager(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector) {
  if (RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled())
    ltp_manager_.emplace(frame_view, paint_timing_detector);
}

void TextRecordsManager::CleanUp() {
  visible_objects_.clear();
  invisible_objects_.clear();
  texts_queued_for_paint_time_.clear();
  CleanUpLargestTextPaint();
}

void TextRecordsManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(text_element_timing_);
  visitor->Trace(ltp_manager_);
}

}  // namespace blink
