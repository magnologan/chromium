// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/persistence/site_data/non_recording_site_data_cache.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {
SiteDataCacheFactory* g_instance = nullptr;
}  // namespace

SiteDataCacheFactory* SiteDataCacheFactory::GetInstance() {
  return g_instance;
}

SiteDataCacheFactory::SiteDataCacheFactory(
    const scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SiteDataCacheFactory::~SiteDataCacheFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
std::unique_ptr<SiteDataCacheFactory, base::OnTaskRunnerDeleter>
SiteDataCacheFactory::Create() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(nullptr, g_instance);
  auto task_runner = PerformanceManager::GetInstance()->task_runner();
  std::unique_ptr<SiteDataCacheFactory, base::OnTaskRunnerDeleter> instance(
      new SiteDataCacheFactory(task_runner),
      base::OnTaskRunnerDeleter(task_runner));

  g_instance = instance.get();
  return instance;
}

// static
std::unique_ptr<SiteDataCacheFactory, base::OnTaskRunnerDeleter>
SiteDataCacheFactory::CreateForTesting(
    const scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK_EQ(nullptr, g_instance);
  std::unique_ptr<SiteDataCacheFactory, base::OnTaskRunnerDeleter> instance(
      new SiteDataCacheFactory(task_runner),
      base::OnTaskRunnerDeleter(task_runner));
  g_instance = instance.get();
  return instance;
}

// static
void SiteDataCacheFactory::OnBrowserContextCreatedOnUIThread(
    SiteDataCacheFactory* factory,
    content::BrowserContext* browser_context,
    content::BrowserContext* parent_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(factory);

  // As |factory| will be deleted on its task runner it's safe to pass the raw
  // pointer to BindOnce, it's guaranteed that this task will run before the
  // factory.
  base::Optional<std::string> parent_context_id;
  if (parent_context) {
    DCHECK(browser_context->IsOffTheRecord());
    parent_context_id = parent_context->UniqueId();
  }
  factory->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SiteDataCacheFactory::OnBrowserContextCreated,
                     base::Unretained(factory), browser_context->UniqueId(),
                     browser_context->GetPath(), parent_context_id));
}

// static
void SiteDataCacheFactory::OnBrowserContextDestroyedOnUIThread(
    SiteDataCacheFactory* factory,
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(factory);
  // See OnBrowserContextCreatedOnUIThread for why it's safe to use a raw
  // pointer in BindOnce.
  factory->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SiteDataCacheFactory::OnBrowserContextDestroyed,
                     base::Unretained(factory), browser_context->UniqueId()));
}

SiteDataCache* SiteDataCacheFactory::GetDataCacheForBrowserContext(
    const std::string& browser_context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_map_.find(browser_context_id);
  if (it != data_cache_map_.end())
    return it->second.get();
  return nullptr;
}

SiteDataCacheInspector* SiteDataCacheFactory::GetInspectorForBrowserContext(
    const std::string& browser_context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_cache_inspector_map_.find(browser_context_id);
  if (it != data_cache_inspector_map_.end())
    return it->second;
  return nullptr;
}

void SiteDataCacheFactory::SetDataCacheInspectorForBrowserContext(
    SiteDataCacheInspector* inspector,
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (inspector) {
    DCHECK_EQ(nullptr, GetInspectorForBrowserContext(browser_context_id));
    data_cache_inspector_map_.emplace(
        std::make_pair(browser_context_id, inspector));
  } else {
    DCHECK_NE(nullptr, GetInspectorForBrowserContext(browser_context_id));
    data_cache_inspector_map_.erase(browser_context_id);
  }
}

void SiteDataCacheFactory::IsDataCacheRecordingForTesting(
    const std::string& browser_context_id,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(
          [](SiteDataCacheFactory* tracker,
             const std::string& browser_context_id) {
            auto it = tracker->data_cache_map_.find(browser_context_id);
            CHECK(it != tracker->data_cache_map_.end());
            return it->second->IsRecordingForTesting();
          },
          this, browser_context_id),
      std::move(cb));
}

void SiteDataCacheFactory::OnBrowserContextCreated(
    const std::string& browser_context_id,
    const base::FilePath& context_path,
    base::Optional<std::string> parent_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!base::Contains(data_cache_map_, browser_context_id));

  if (parent_context_id) {
    SiteDataCacheInspector* parent_debug =
        GetInspectorForBrowserContext(parent_context_id.value());
    DCHECK(parent_debug);
    DCHECK(base::Contains(data_cache_map_, parent_context_id.value()));
    SiteDataCache* data_cache_for_readers =
        data_cache_map_[parent_context_id.value()].get();
    DCHECK(data_cache_for_readers);
    data_cache_map_.emplace(std::make_pair(
        std::move(browser_context_id),
        std::make_unique<NonRecordingSiteDataCache>(
            browser_context_id, parent_debug, data_cache_for_readers)));
  } else {
    data_cache_map_.emplace(std::make_pair(
        std::move(browser_context_id),
        std::make_unique<SiteDataCacheImpl>(browser_context_id, context_path)));
  }
}

void SiteDataCacheFactory::OnBrowserContextDestroyed(
    const std::string& browser_context_id) {
  DCHECK(base::Contains(data_cache_map_, browser_context_id));
  data_cache_map_.erase(browser_context_id);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace performance_manager
