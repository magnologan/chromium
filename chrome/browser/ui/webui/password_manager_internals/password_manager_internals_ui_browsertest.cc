// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "content/public/browser/web_contents.h"

class PasswordManagerInternalsWebUIBrowserTest : public WebUIBrowserTest {
 public:
  PasswordManagerInternalsWebUIBrowserTest();
  ~PasswordManagerInternalsWebUIBrowserTest() override;

  void SetUpOnMainThread() override;

 protected:
  content::WebContents* GetWebContents();

  // Navigates to the internals page in a tab specified by |disposition| (and
  // optionally, by |browser|). Also assigns the corresponding UI controller to
  // |controller_|.
  void OpenInternalsPage(WindowOpenDisposition disposition);
  void OpenInternalsPageWithBrowser(Browser* browser,
                                    WindowOpenDisposition disposition);

 private:
  PasswordManagerInternalsUI* controller_;
};

PasswordManagerInternalsWebUIBrowserTest::
    PasswordManagerInternalsWebUIBrowserTest()
    : controller_(NULL) {}

PasswordManagerInternalsWebUIBrowserTest::
    ~PasswordManagerInternalsWebUIBrowserTest() {}

void PasswordManagerInternalsWebUIBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
  OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);
  AddLibrary(base::FilePath(
      FILE_PATH_LITERAL("password_manager_internals_browsertest.js")));
}

content::WebContents*
PasswordManagerInternalsWebUIBrowserTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void PasswordManagerInternalsWebUIBrowserTest::OpenInternalsPage(
    WindowOpenDisposition disposition) {
  OpenInternalsPageWithBrowser(browser(), disposition);
}

void PasswordManagerInternalsWebUIBrowserTest::OpenInternalsPageWithBrowser(
    Browser* browser,
    WindowOpenDisposition disposition) {
  std::string url_string("chrome://");
  url_string += chrome::kChromeUIPasswordManagerInternalsHost;
  ui_test_utils::NavigateToURLWithDisposition(
      browser,
      GURL(url_string),
      disposition,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  controller_ = static_cast<PasswordManagerInternalsUI*>(
      GetWebContents()->GetWebUI()->GetController());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest, LogEntry) {
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  service->ProcessLog("<script> text for testing");
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}

// Test that a single internals page is empty on load.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_EmptyOnLoad) {
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  ASSERT_TRUE(RunJavascriptTest("testLogEmpty"));
}

// Test that a single internals page is flushed on reload.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_FlushedOnReload) {
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  service->ProcessLog("<script> text for testing");
  OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);  // Reload.
  ASSERT_TRUE(RunJavascriptTest("testLogEmpty"));
}

// Test that if two tabs with the internals page are open, the second displays
// the same logs. In particular, this checks that both the second tab gets the
// logs created before the second tab was opened, and also that the second tab
// waits with displaying until the internals page is ready (trying to display
// the old logs just on construction time would fail).
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_MultipleTabsIdentical) {
  // First, open one tab with the internals page, and log something.
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  service->ProcessLog("<script> text for testing");
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
  // Now open a second tab with the internals page, but do not log anything.
  OpenInternalsPage(WindowOpenDisposition::NEW_FOREGROUND_TAB);
  // The previously logged text should have made it to the page.
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}

// Test that in the presence of more internals pages, reload does not cause
// flushing the logs.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_NotFlushedOnReloadIfMultiple) {
  // Open one more tab with the internals page.
  OpenInternalsPage(WindowOpenDisposition::NEW_FOREGROUND_TAB);
  // Now log something.
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  service->ProcessLog("<script> text for testing");
  // Reload.
  OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);
  // The text should still be there.
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}

// Test that navigation away from the internals page works OK.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_NavigateAway) {
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  service->ProcessLog("<script> text for testing");
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
}

// Test that the description is correct in a non-Incognito tab.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       NonIncognitoMessage) {
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);
  ASSERT_TRUE(RunJavascriptTest("testNonIncognitoDescription"));
}

// Test that the description is correct in an Incognito tab.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       IncognitoMessage) {
  Browser* incognito = CreateIncognitoBrowser();
  password_manager::PasswordManagerInternalsService* service =
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(incognito->profile()->GetOffTheRecordProfile());
  EXPECT_FALSE(service);  // There should be no service for Incognito.
  OpenInternalsPageWithBrowser(incognito, WindowOpenDisposition::CURRENT_TAB);
  SetWebUIInstance(
      incognito->tab_strip_model()->GetActiveWebContents()->GetWebUI());
  ASSERT_TRUE(RunJavascriptTest("testIncognitoDescription"));
}
