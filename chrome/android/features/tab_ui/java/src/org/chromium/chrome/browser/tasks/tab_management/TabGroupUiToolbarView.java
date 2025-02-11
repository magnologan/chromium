// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v4.widget.TextViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Represents a generic toolbar used in the bottom strip/grid component.
 * {@link TabGridSheetToolbarCoordinator}
 */
public class TabGroupUiToolbarView extends FrameLayout {
    private ChromeImageView mRightButton;
    private ChromeImageView mLeftButton;
    private ViewGroup mContainerView;
    private TextView mTitleTextView;
    private View mMainContent;

    public TabGroupUiToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mLeftButton = findViewById(R.id.toolbar_left_button);
        mRightButton = findViewById(R.id.toolbar_right_button);
        mContainerView = (ViewGroup) findViewById(R.id.toolbar_container_view);
        mTitleTextView = (TextView) findViewById(R.id.title);
        mMainContent = findViewById(R.id.main_content);
    }

    void setLeftButtonOnClickListener(OnClickListener listener) {
        mLeftButton.setOnClickListener(listener);
    }

    void setRightButtonOnClickListener(OnClickListener listener) {
        mRightButton.setOnClickListener(listener);
    }

    ViewGroup getViewContainer() {
        return mContainerView;
    }

    void setMainContentVisibility(boolean isVisible) {
        if (mContainerView == null)
            throw new IllegalStateException("Current Toolbar doesn't have a container view");

        for (int i = 0; i < ((ViewGroup) mContainerView).getChildCount(); i++) {
            View child = ((ViewGroup) mContainerView).getChildAt(i);
            child.setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
        }
    }

    void setTitle(String title) {
        if (mTitleTextView == null)
            throw new IllegalStateException("Current Toolbar doesn't have a title text view");

        mTitleTextView.setText(title);
    }

    void setPrimaryColor(int color) {
        mMainContent.setBackgroundColor(color);
    }

    void setTint(ColorStateList tint) {
        ApiCompatibilityUtils.setImageTintList(mLeftButton, tint);
        ApiCompatibilityUtils.setImageTintList(mRightButton, tint);
        if (mTitleTextView != null) mTitleTextView.setTextColor(tint);
    }

    /**
     * Setup a TabGridDialog-specific toolbar. It is different from the toolbar for TabGridSheet.
     */
    void setupDialogToolbarLayout() {
        Context context = getContext();
        setBackground(AppCompatResources.getDrawable(context, R.drawable.tab_grid_card_background));
        mLeftButton.setImageResource(org.chromium.chrome.R.drawable.ic_arrow_back_24dp);
        int topicMargin =
                (int) context.getResources().getDimension(R.dimen.tab_group_toolbar_topic_margin);
        MarginLayoutParams params = (MarginLayoutParams) mTitleTextView.getLayoutParams();
        params.setMarginStart(topicMargin);
        mTitleTextView.setGravity(Gravity.START);
        TextViewCompat.setTextAppearance(
                mTitleTextView, org.chromium.chrome.R.style.TextAppearance_BlackHeadline);
    }
}
