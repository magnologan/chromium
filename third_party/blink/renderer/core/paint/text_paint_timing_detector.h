// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_

#include <memory>
#include <queue>
#include <set>

#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/paint/text_element_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class LayoutBoxModelObject;
class LocalFrameView;
class PropertyTreeState;
class TextElementTiming;
class TracedValue;

class TextRecord : public base::SupportsWeakPtr<TextRecord> {
 public:
  TextRecord(DOMNodeId new_node_id,
             uint64_t new_first_size,
             const FloatRect& element_timing_rect)
      : node_id(new_node_id),
        first_size(new_first_size),
        element_timing_rect_(element_timing_rect) {
    static unsigned next_insertion_index_ = 1;
    insertion_index_ = next_insertion_index_++;
  }

  DOMNodeId node_id = kInvalidDOMNodeId;
  uint64_t first_size = 0;
  // |insertion_index_| is ordered by insertion time, used as a secondary key
  // for ranking.
  unsigned insertion_index_ = 0;
  FloatRect element_timing_rect_;
  // The time of the first paint after fully loaded.
  base::TimeTicks paint_time = base::TimeTicks();
  DISALLOW_COPY_AND_ASSIGN(TextRecord);
};

class CORE_EXPORT LargestTextPaintManager {
  DISALLOW_NEW();
  using TextRecordSetComparator = bool (*)(const base::WeakPtr<TextRecord>&,
                                           const base::WeakPtr<TextRecord>&);
  using TextRecordSet =
      std::set<base::WeakPtr<TextRecord>, TextRecordSetComparator>;

 public:
  LargestTextPaintManager(LocalFrameView*, PaintTimingDetector*);

  inline void RemoveVisibleRecord(base::WeakPtr<TextRecord> record) {
    size_ordered_set_.erase(record);
    if (cached_largest_paint_candidate_.get() == record.get())
      cached_largest_paint_candidate_ = nullptr;
    is_result_invalidated_ = true;
  }

  base::WeakPtr<TextRecord> FindLargestPaintCandidate();

  void ReportCandidateToTrace(const TextRecord&);
  void ReportNoCandidateToTrace();
  void UpdateCandidate();
  void PopulateTraceValue(TracedValue&, const TextRecord& first_text_paint);
  inline void SetCachedResultInvalidated(bool value) {
    is_result_invalidated_ = value;
  }

  inline void InsertRecord(base::WeakPtr<TextRecord> record) {
    size_ordered_set_.emplace(record);
    SetCachedResultInvalidated(true);
  }

  void Trace(blink::Visitor*);

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class TextPaintTimingDetectorTest;

  TextRecordSet size_ordered_set_;
  base::WeakPtr<TextRecord> cached_largest_paint_candidate_;
  // This is used to cache the largest text paint result for better
  // efficiency.
  // The result will be invalidated whenever any change is done to the
  // variables used in |FindLargestPaintCandidate|.
  bool is_result_invalidated_ = false;
  unsigned count_candidates_ = 0;

  Member<const LocalFrameView> frame_view_;
  Member<PaintTimingDetector> paint_timing_detector_;
  DISALLOW_COPY_AND_ASSIGN(LargestTextPaintManager);
};

class CORE_EXPORT TextRecordsManager {
  DISALLOW_NEW();
  friend class TextPaintTimingDetectorTest;

 public:
  TextRecordsManager(LocalFrameView*, PaintTimingDetector*);

  void RemoveVisibleRecord(const LayoutObject&);
  void RemoveInvisibleRecord(const LayoutObject&);
  inline void RecordInvisibleObject(const LayoutObject& object) {
    invisible_objects_.insert(&object);
  }
  void RecordVisibleObject(const LayoutObject&,
                           const uint64_t& visual_size,
                           const FloatRect& element_timing_rect);
  bool NeedMeausuringPaintTime() const {
    return !texts_queued_for_paint_time_.IsEmpty();
  }
  void AssignPaintTimeToQueuedRecords(const base::TimeTicks&);

  inline bool HasRecorded(const LayoutObject& object) const {
    return visible_objects_.Contains(&object) ||
           invisible_objects_.Contains(&object);
  }

  size_t CountVisibleObjects() const { return visible_objects_.size(); }
  size_t CountInvisibleObjects() const { return invisible_objects_.size(); }

  inline bool IsKnownVisible(const LayoutObject& object) const {
    return visible_objects_.Contains(&object);
  }

  inline bool IsKnownInvisible(const LayoutObject& object) const {
    return invisible_objects_.Contains(&object);
  }

  void CleanUp();
  void CleanUpLargestTextPaint();
  void StopRecordingLargestTextPaint();

  bool HasTextElementTiming() const { return text_element_timing_; }
  void SetTextElementTiming(TextElementTiming* text_element_timing) {
    text_element_timing_ = text_element_timing;
  }

  inline base::Optional<LargestTextPaintManager>& GetLargestTextPaintManager() {
    return ltp_manager_;
  }

  inline bool IsRecordingLargestTextPaint() const {
    return ltp_manager_.has_value();
  }

  void Trace(blink::Visitor*);

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class TextPaintTimingDetectorTest;
  inline void QueueToMeasurePaintTime(base::WeakPtr<TextRecord>& record) {
    texts_queued_for_paint_time_.push_back(std::move(record));
  }

  // Once LayoutObject* is destroyed, |visible_objects_| and
  // |invisible_objects_| must immediately clear the corresponding record from
  // themselves.
  HashMap<const LayoutObject*, std::unique_ptr<TextRecord>> visible_objects_;
  HashSet<const LayoutObject*> invisible_objects_;

  Deque<base::WeakPtr<TextRecord>> texts_queued_for_paint_time_;
  base::Optional<LargestTextPaintManager> ltp_manager_;
  Member<TextElementTiming> text_element_timing_;

  DISALLOW_COPY_AND_ASSIGN(TextRecordsManager);
};

// TextPaintTimingDetector contains Largest Text Paint and support for Text
// Element Timing.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within viewport. Specifically, it:
// 1. Tracks all texts' first paints, recording their visual size, paint time.
// 2. Every 1 second after the first text pre-paint, the algorithm starts an
// analysis. In the analysis:
// 2.1 Largest Text Paint finds the text with the  largest first visual size,
// reports its first paint time as a candidate result.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Text Paint candidate as the final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.lvno2v283uls
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollectedFinalized<TextPaintTimingDetector> {
  using ReportTimeCallback =
      WTF::CrossThreadOnceFunction<void(WebWidgetClient::SwapResult,
                                        base::TimeTicks)>;
  friend class TextPaintTimingDetectorTest;

 public:
  explicit TextPaintTimingDetector(LocalFrameView*, PaintTimingDetector*);
  bool ShouldWalkObject(const LayoutBoxModelObject&) const;
  void RecordAggregatedText(const LayoutBoxModelObject& aggregator,
                            const IntRect& aggregated_visual_rect,
                            const PropertyTreeState&);
  void OnPaintFinished();
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void StopRecordEntries();
  void StopRecordingLargestTextPaint();
  bool IsRecording() const { return is_recording_; }
  inline bool FinishedReportingText() const { return !is_recording_; }
  void Trace(blink::Visitor*);

 private:
  friend class LargestContentfulPaintCalculatorTest;

  void ReportSwapTime(WebWidgetClient::SwapResult result,
                      base::TimeTicks timestamp);
  void RegisterNotifySwapTime(ReportTimeCallback callback);

  TextRecordsManager records_manager_;

  // Make sure that at most one swap promise is ongoing.
  bool awaiting_swap_promise_ = false;
  bool is_recording_ = true;

  bool need_update_timing_at_frame_end_ = false;

  Member<const LocalFrameView> frame_view_;

  DISALLOW_COPY_AND_ASSIGN(TextPaintTimingDetector);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
