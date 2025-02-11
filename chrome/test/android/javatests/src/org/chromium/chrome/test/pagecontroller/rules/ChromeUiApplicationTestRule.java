// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.rules;

import android.support.test.InstrumentationRegistry;

import org.junit.rules.ExternalResource;

import org.chromium.base.Log;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.android.PermissionDialog;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.DataSaverController;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.SyncController;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.TOSController;
import org.chromium.chrome.test.pagecontroller.controllers.notifications.DownloadNotificationController;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.ChromeMenu;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.IncognitoNewTabPageController;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;

/**
 * Test rule that provides access to a Chrome application.
 */
public class ChromeUiApplicationTestRule extends ExternalResource {
    // TODO(aluo): Adjust according to https://crrev.com/c/1585142.
    public static final String PACKAGE_NAME_ARG = "PackageUnderTest";
    private static final String TAG = "ChromeUiAppTR";

    private String mPackageName;

    /**
     * Returns an instance of the page controller that corresponds to the current page.
     * @param controllers      List of possible page controller instances to search among.
     * @return                 The detected page controller.
     * @throws UiLocationError If page can't be determined.
     */
    public static PageController detectPageAmong(PageController... controllers) {
        for (PageController instance : controllers) {
            if (instance.isCurrentPageThis()) {
                Log.d(TAG, "Detected " + instance.getClass().getName());
                return instance;
            }
        }
        throw UiLocationException.newInstance("Could not detect current Page");
    }

    /**
     * Detect the page controller from among all page controllers.
     * When a new page controller is implemented, add it to the list here.
     * @return                 The detected page controller.
     * @throws UiLocationError If page can't be determined.
     */
    public static PageController detectCurrentPage() {
        return detectPageAmong(NewTabPageController.getInstance(), SyncController.getInstance(),
                DataSaverController.getInstance(), TOSController.getInstance(),
                DownloadNotificationController.getInstance(), PermissionDialog.getInstance(),
                IncognitoNewTabPageController.getInstance(), ChromeMenu.getInstance());
    }

    /** Launch the chrome application */
    public void launchApplication() {
        UiAutomatorUtils utils = UiAutomatorUtils.getInstance();
        utils.launchApplication(mPackageName);
    }

    /** Navigate to the New Tab Page from somewhere in the application. */
    public NewTabPageController navigateToNewTabPage() {
        PageController controller = detectCurrentPage();
        if (controller instanceof TOSController) {
            ((TOSController) controller).acceptAndContinue();
            controller = detectCurrentPage();
        }
        if (controller instanceof DataSaverController) {
            ((DataSaverController) controller).clickNext();
            controller = detectCurrentPage();
        }
        if (controller instanceof SyncController) {
            ((SyncController) controller).clickNoThanks();
            controller = detectCurrentPage();
        }
        if (controller instanceof ChromeMenu) {
            controller = ((ChromeMenu) controller).dismiss();
        }
        if (controller instanceof NewTabPageController) {
            return (NewTabPageController) controller;
        } else {
            throw UiLocationException.newInstance(
                    "Could not navigate to new tab page from " + controller.getClass().getName());
        }
    }

    /** Launch the application and navigate to the New Tab Page */
    public NewTabPageController launchIntoNewTabPage() {
        launchApplication();
        return navigateToNewTabPage();
    }

    public String getApplicationPackage() {
        return mPackageName;
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        mPackageName = InstrumentationRegistry.getArguments().getString(PACKAGE_NAME_ARG);
        // If the runner didn't pass the package name under test to us, then we can assume
        // that the target package name provided in the AndroidManifest is the app-under-test.
        if (mPackageName == null) {
            mPackageName = InstrumentationRegistry.getTargetContext().getPackageName();
        }
    }
}
