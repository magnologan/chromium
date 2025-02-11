// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_PAGES_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_PAGES_TASK_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {
class ClientPolicyController;

// Gets offline pages that match the criteria.
class GetPagesTask : public Task {
 public:
  // Structure defining and intermediate read result.
  struct ReadResult {
    ReadResult();
    ReadResult(const ReadResult& other);
    ~ReadResult();

    bool success = false;
    std::vector<OfflinePageItem> pages;
  };

  GetPagesTask(OfflinePageMetadataStore* store,
               const ClientPolicyController* policy_controller,
               const PageCriteria& criteria,
               MultipleOfflinePageItemCallback callback);

  ~GetPagesTask() override;

  // Task implementation:
  void Run() override;

  // Reads and returns all pages matching |criteria|. This function reads
  // from the database and should be called from within an
  // |SqlStoreBase::Execute()| call.
  static ReadResult ReadPagesWithCriteriaSync(
      const ClientPolicyController* policy_controller,
      const PageCriteria& criteria,
      sql::Database* db);

 private:
  void CompleteWithResult(ReadResult result);

  OfflinePageMetadataStore* store_;
  const ClientPolicyController* policy_controller_;
  PageCriteria criteria_;
  MultipleOfflinePageItemCallback callback_;

  base::WeakPtrFactory<GetPagesTask> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(GetPagesTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_GET_PAGES_TASK_H_
