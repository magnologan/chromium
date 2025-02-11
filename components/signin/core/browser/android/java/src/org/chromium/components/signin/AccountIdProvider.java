// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.util.List;

/**
 * Returns a stable id that can be used to identify a Google Account.  This
 * id does not change if the email address associated to the account changes,
 * nor does it change depending on whether the email has dots or varying
 * capitalization.
 *
 * TODO(https://crbug.com/831257): remove this class after all clients start using {@link
 * AccountManagerFacade} methods instead.
 */
public class AccountIdProvider {
    private static AccountIdProvider sProvider;

    protected AccountIdProvider() {
        // should not be initialized outside getInstance().
    }

    /**
     * Returns a stable id for the account associated with the given email address.
     * If an account with the given email address is not installed on the device
     * then null is returned.
     *
     * This method will throw IllegalStateException if called on the main thread.
     *
     * @param accountName The email address of a Google account.
     */
    public String getAccountId(String accountName) {
        List<CoreAccountInfo> accountInfos = AccountManagerFacade.get().tryGetAccounts();
        if (accountInfos == null) {
            return null;
        }
        for (CoreAccountInfo accountInfo : accountInfos) {
            if (accountInfo.getName().equals(accountName)) {
                return accountInfo.getId().getGaiaIdAsString();
            }
        }
        return null;
    }

    /**
     * Returns whether the AccountIdProvider can be used.
     * Since the AccountIdProvider queries Google Play services, this basically checks whether
     * Google Play services is available.
     */
    public boolean canBeUsed() {
        // TODO(http://crbug.com/577190): Remove StrictMode override.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            int resultCode = GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                    ContextUtils.getApplicationContext());
            return resultCode == ConnectionResult.SUCCESS;
        }
    }

    /**
     * Gets the global account Id provider.
     */
    public static AccountIdProvider getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sProvider == null) sProvider = new AccountIdProvider();
        return sProvider;
    }

    /**
     * For testing purposes only, allows to set the provider even if it has already been
     * initialized.
     */
    @VisibleForTesting
    public static void setInstanceForTest(AccountIdProvider provider) {
        ThreadUtils.assertOnUiThread();
        sProvider = provider;
    }
}
