// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_processing_util.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/hint_update_data.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "url/gurl.h"

namespace optimization_guide {

bool IsDisabledPerOptimizationHintExperiment(
    const proto::Optimization& optimization) {
  // First check if optimization depends on an experiment being enabled.
  if (optimization.has_experiment_name() &&
      !optimization.experiment_name().empty() &&
      optimization.experiment_name() !=
          base::GetFieldTrialParamValueByFeature(
              features::kOptimizationHintsExperiments,
              features::kOptimizationHintsExperimentNameParam)) {
    return true;
  }
  // Now check if optimization depends on an experiment not being enabled.
  if (optimization.has_excluded_experiment_name() &&
      !optimization.excluded_experiment_name().empty() &&
      optimization.excluded_experiment_name() ==
          base::GetFieldTrialParamValueByFeature(
              features::kOptimizationHintsExperiments,
              features::kOptimizationHintsExperimentNameParam)) {
    return true;
  }
  return false;
}

const proto::PageHint* FindPageHintForURL(const GURL& gurl,
                                          const proto::Hint* hint) {
  if (!hint) {
    return nullptr;
  }

  for (const auto& page_hint : hint->page_hints()) {
    if (page_hint.page_pattern().empty()) {
      continue;
    }
    URLPatternWithWildcards url_pattern(page_hint.page_pattern());
    if (url_pattern.Matches(gurl.spec())) {
      // Return the first matching page hint.
      return &page_hint;
    }
  }
  return nullptr;
}

std::string HashHostForDictionary(const std::string& host) {
  return base::StringPrintf("%x", base::PersistentHash(host));
}

bool ProcessHints(
    google::protobuf::RepeatedPtrField<optimization_guide::proto::Hint>* hints,
    optimization_guide::HintUpdateData* hint_update_data) {
  // If there's no update data, then there's nothing to do.
  if (!hint_update_data)
    return false;

  std::unordered_set<std::string> seen_host_suffixes;

  bool did_process_hints = false;
  // Process each hint in the the hint configuration. The hints are mutable
  // because once processing is completed on each individual hint, it is moved
  // into the component update data. This eliminates the need to make any
  // additional copies of the hints.
  for (auto& hint : *hints) {
    // We only support host suffixes at the moment. Skip anything else.
    // One |hint| applies to one host URL suffix.
    if (hint.key_representation() != optimization_guide::proto::HOST_SUFFIX) {
      continue;
    }

    const std::string& hint_key = hint.key();

    // Validate configuration keys.
    DCHECK(!hint_key.empty());
    if (hint_key.empty()) {
      continue;
    }

    auto seen_host_suffixes_iter = seen_host_suffixes.find(hint_key);
    DCHECK(seen_host_suffixes_iter == seen_host_suffixes.end());
    if (seen_host_suffixes_iter != seen_host_suffixes.end()) {
      DLOG(WARNING) << "Received config with duplicate key";
      continue;
    }
    seen_host_suffixes.insert(hint_key);

    if (!hint.page_hints().empty()) {
      // Now that processing is finished on |hint|, move it into the update
      // data.
      // WARNING: Do not use |hint| after this call. Its contents will no
      // longer be valid.
      hint_update_data->MoveHintIntoUpdateData(std::move(hint));
      did_process_hints = true;
    }
  }

  return did_process_hints;
}

}  // namespace optimization_guide
