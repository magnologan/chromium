// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIGNED_EXCHANGE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIGNED_EXCHANGE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramSignedExchangePrefix[];
extern const char kHistogramSignedExchangeParseStart[];
extern const char kHistogramSignedExchangeFirstInputDelay[];
extern const char kHistogramSignedExchangeFirstPaint[];
extern const char kHistogramSignedExchangeFirstContentfulPaint[];
extern const char kHistogramSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramSignedExchangeFirstMeaningfulPaint[];
extern const char kHistogramSignedExchangeParseStartToFirstMeaningfulPaint[];
extern const char kHistogramSignedExchangeDomContentLoaded[];
extern const char kHistogramSignedExchangeLoad[];

extern const char kHistogramCachedSignedExchangePrefix[];
extern const char kHistogramCachedSignedExchangeParseStart[];
extern const char kHistogramCachedSignedExchangeFirstInputDelay[];
extern const char kHistogramCachedSignedExchangeFirstPaint[];
extern const char kHistogramCachedSignedExchangeFirstContentfulPaint[];
extern const char
    kHistogramCachedSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramCachedSignedExchangeFirstMeaningfulPaint[];
extern const char
    kHistogramCachedSignedExchangeParseStartToFirstMeaningfulPaint[];
extern const char kHistogramCachedSignedExchangeDomContentLoaded[];
extern const char kHistogramCachedSignedExchangeLoad[];

extern const char kHistogramAltSubSxgSignedExchangePrefix[];
extern const char kHistogramAltSubSxgSignedExchangeParseStart[];
extern const char kHistogramAltSubSxgSignedExchangeFirstInputDelay[];
extern const char kHistogramAltSubSxgSignedExchangeFirstPaint[];
extern const char kHistogramAltSubSxgSignedExchangeFirstContentfulPaint[];
extern const char
    kHistogramAltSubSxgSignedExchangeParseStartToFirstContentfulPaint[];
extern const char kHistogramAltSubSxgSignedExchangeFirstMeaningfulPaint[];
extern const char
    kHistogramAltSubSxgSignedExchangeParseStartToFirstMeaningfulPaint[];
extern const char kHistogramAltSubSxgSignedExchangeDomContentLoaded[];
extern const char kHistogramAltSubSxgSignedExchangeLoad[];

}  // namespace internal

class SignedExchangePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  SignedExchangePageLoadMetricsObserver();
  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  // True iff the page main resource was served from disk cache.
  bool was_cached_ = false;

  // True iff prefetched alternative signed exchange was sent to the renderer
  // process.
  bool had_prefetched_alt_sxg_ = false;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SIGNED_EXCHANGE_PAGE_LOAD_METRICS_OBSERVER_H_
