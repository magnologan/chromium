// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.ui.widget.ChipView;

/**
 * This view represents a section of user credit card details in the payment method tab of the
 * keyboard accessory (manual fallback) sheet.
 */

class CreditCardAccessoryInfoView extends LinearLayout {
    private ImageView mIcon;
    private ChipView mCCNumber;
    private ChipView mExpMonth;
    private ChipView mExpYear;
    private ChipView mCardholder;

    /**
     * Constructor for inflating from XML.
     */
    public CreditCardAccessoryInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIcon = findViewById(R.id.icon);
        mCCNumber = findViewById(R.id.cc_number);
        mExpMonth = findViewById(R.id.exp_month);
        mExpYear = findViewById(R.id.exp_year);
        mCardholder = findViewById(R.id.cardholder);
    }

    public void setIcon(@Nullable Drawable drawable) {
        if (drawable == null) {
            mIcon.setVisibility(View.GONE);
            return;
        }
        mIcon.setVisibility(View.VISIBLE);
        mIcon.setImageDrawable(drawable);
    }

    public ChipView getCCNumber() {
        return mCCNumber;
    }

    public ChipView getExpMonth() {
        return mExpMonth;
    }

    public ChipView getExpYear() {
        return mExpYear;
    }

    public ChipView getCardholder() {
        return mCardholder;
    }
}
