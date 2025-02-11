// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test_utils.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

base::FilePath GetTestFilePath(const char* dir, const char* file) {
  base::FilePath path;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::PathService::Get(DIR_TEST_DATA, &path);
  if (dir)
    path = path.AppendASCII(dir);
  return path.AppendASCII(file);
}

GURL GetTestUrl(const char* dir, const char* file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

void NavigateToURLBlockUntilNavigationsComplete(Shell* window,
                                                const GURL& url,
                                                int number_of_navigations) {
  NavigateToURLBlockUntilNavigationsComplete(window->web_contents(), url,
                                             number_of_navigations);
}

void ReloadBlockUntilNavigationsComplete(Shell* window,
                                         int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->Reload();
  same_tab_observer.Wait();
}

void ReloadBypassingCacheBlockUntilNavigationsComplete(
    Shell* window,
    int number_of_navigations) {
  WaitForLoadStop(window->web_contents());
  TestNavigationObserver same_tab_observer(window->web_contents(),
                                           number_of_navigations);

  window->ReloadBypassingCache();
  same_tab_observer.Wait();
}

bool NavigateToURL(Shell* window, const GURL& url) {
  return NavigateToURL(window->web_contents(), url);
}

bool NavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                               const GURL& url) {
  RenderFrameHost* rfh = adapter.render_frame_host();
  TestFrameNavigationObserver nav_observer(rfh);
  if (!ExecJs(rfh, JsReplace("location = $1", url)))
    return false;
  nav_observer.Wait();
  return nav_observer.last_committed_url() == url &&
         nav_observer.last_navigation_succeeded();
}

bool NavigateToURLFromRendererWithoutUserGesture(
    const ToRenderFrameHost& adapter,
    const GURL& url) {
  RenderFrameHost* rfh = adapter.render_frame_host();
  TestFrameNavigationObserver nav_observer(rfh);
  if (!ExecJs(rfh, JsReplace("location = $1", url),
              EXECUTE_SCRIPT_NO_USER_GESTURE)) {
    return false;
  }
  nav_observer.Wait();
  return nav_observer.last_committed_url() == url;
}

bool NavigateToURLAndExpectNoCommit(Shell* window, const GURL& url) {
  NavigationEntry* old_entry =
      window->web_contents()->GetController().GetLastCommittedEntry();
  NavigateToURLBlockUntilNavigationsComplete(window, url, 1);
  NavigationEntry* new_entry =
      window->web_contents()->GetController().GetLastCommittedEntry();
  return old_entry == new_entry;
}

void WaitForAppModalDialog(Shell* window) {
  ShellJavaScriptDialogManager* dialog_manager =
      static_cast<ShellJavaScriptDialogManager*>(
          window->GetJavaScriptDialogManager(window->web_contents()));

  base::RunLoop runner;
  dialog_manager->set_dialog_request_callback(runner.QuitClosure());
  runner.Run();
}

RenderFrameHost* ConvertToRenderFrameHost(Shell* shell) {
  return shell->web_contents()->GetMainFrame();
}

void LookupAndLogNameAndIdOfFirstCamera() {
  DCHECK(BrowserMainLoop::GetInstance());
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  base::RunLoop run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          [](MediaStreamManager* media_stream_manager,
             base::Closure quit_closure) {
            media_stream_manager->video_capture_manager()->EnumerateDevices(
                base::BindOnce(
                    [](base::Closure quit_closure,
                       const media::VideoCaptureDeviceDescriptors&
                           descriptors) {
                      if (descriptors.empty()) {
                        LOG(WARNING) << "No camera found";
                        return;
                      }
                      LOG(INFO) << "Using camera "
                                << descriptors.front().display_name() << " ("
                                << descriptors.front().model_id << ")";
                      std::move(quit_closure).Run();
                    },
                    std::move(quit_closure)));
          },
          media_stream_manager, run_loop.QuitClosure()));
  run_loop.Run();
}

ShellAddedObserver::ShellAddedObserver() {
  Shell::SetShellCreatedCallback(base::BindOnce(
      &ShellAddedObserver::ShellCreated, base::Unretained(this)));
}

ShellAddedObserver::~ShellAddedObserver() = default;

Shell* ShellAddedObserver::GetShell() {
  if (shell_)
    return shell_;

  runner_ = std::make_unique<base::RunLoop>();
  runner_->Run();
  return shell_;
}

void ShellAddedObserver::ShellCreated(Shell* shell) {
  DCHECK(!shell_);
  shell_ = shell;
  if (runner_)
    runner_->Quit();
}

void IsolateOriginsForTesting(
    net::test_server::EmbeddedTestServer* embedded_test_server,
    WebContents* web_contents,
    std::vector<std::string> hostnames_to_isolate) {
  std::vector<url::Origin> origins_to_isolate;
  for (const std::string& hostname : hostnames_to_isolate) {
    origins_to_isolate.push_back(
        url::Origin::Create(GURL(std::string("http://") + hostname + "/")));
  }

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins(
      origins_to_isolate,
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  // Force a BrowsingInstance swap by navigating cross-site (the newly
  // isolated origin only affects *future* BrowsingInstances).
  scoped_refptr<SiteInstanceImpl> old_site_instance;
  scoped_refptr<SiteInstanceImpl> new_site_instance;
  do {
    old_site_instance = static_cast<SiteInstanceImpl*>(
        web_contents->GetMainFrame()->GetSiteInstance());
    std::string cross_site_hostname = base::GenerateGUID() + ".com";
    EXPECT_TRUE(NavigateToURL(
        web_contents,
        embedded_test_server->GetURL(cross_site_hostname, "/title1.html")));
    new_site_instance = static_cast<SiteInstanceImpl*>(
        web_contents->GetMainFrame()->GetSiteInstance());

    // The navigation might need to be repeated until we actually swap the
    // SiteInstance (no swap might happen when navigating away from the initial,
    // empty frame).
  } while (new_site_instance.get() == old_site_instance.get());
  EXPECT_FALSE(
      new_site_instance->IsRelatedSiteInstance(old_site_instance.get()));
  for (const url::Origin& origin : origins_to_isolate) {
    EXPECT_FALSE(policy->IsIsolatedOrigin(
        old_site_instance->GetIsolationContext(), origin));
    EXPECT_TRUE(policy->IsIsolatedOrigin(
        new_site_instance->GetIsolationContext(), origin));
  }
}

}  // namespace content
