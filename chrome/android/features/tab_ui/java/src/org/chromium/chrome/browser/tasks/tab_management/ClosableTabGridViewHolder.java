// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.support.annotation.IntDef;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.tab_ui.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/**
 * A {@link TabGridViewHolder} with a close button. This is used in the Grid Tab Switcher.
 */
class ClosableTabGridViewHolder extends TabGridViewHolder {
    public static final long RESTORE_ANIMATION_DURATION_MS = 50;
    public static final float ZOOM_IN_SCALE = 0.8f;
    @IntDef({AnimationStatus.SELECTED_CARD_ZOOM_IN, AnimationStatus.SELECTED_CARD_ZOOM_OUT,
            AnimationStatus.HOVERED_CARD_ZOOM_IN, AnimationStatus.HOVERED_CARD_ZOOM_OUT,
            AnimationStatus.CARD_RESTORE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnimationStatus {
        int CARD_RESTORE = 0;
        int SELECTED_CARD_ZOOM_OUT = 1;
        int SELECTED_CARD_ZOOM_IN = 2;
        int HOVERED_CARD_ZOOM_OUT = 3;
        int HOVERED_CARD_ZOOM_IN = 4;
        int NUM_ENTRIES = 5;
    }

    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;
    private boolean mIsAnimating;

    ClosableTabGridViewHolder(View itemView) {
        super(itemView);
        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize =
                    (int) itemView.getResources().getDimension(R.dimen.tab_grid_close_button_size);
            Bitmap bitmap =
                    BitmapFactory.decodeResource(itemView.getResources(), R.drawable.btn_close);
            sCloseButtonBitmapWeakRef = new WeakReference<>(
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true));
            bitmap.recycle();
        }

        actionButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }

    /**
     * Play the zoom-in and zoom-out animations for tab grid card.
     * @param status      The target animation status in {@link AnimationStatus}.
     * @param isSelected  Whether the scaling card is selected or not.
     */
    void scaleTabGridCardView(@AnimationStatus int status, boolean isSelected) {
        assert status < AnimationStatus.NUM_ENTRIES;

        View view = itemView;
        Context context = view.getContext();
        final View backgroundView = view.findViewById(R.id.background_view);
        final View contentView = view.findViewById(R.id.content_view);
        final int cardNormalMargin =
                (int) context.getResources().getDimension(R.dimen.tab_list_card_padding);
        final int cardBackgroundMargin =
                (int) context.getResources().getDimension(R.dimen.tab_list_card_background_margin);
        final Drawable greyBackground =
                AppCompatResources.getDrawable(context, R.drawable.tab_grid_card_background_grey);
        final Drawable normalBackground =
                AppCompatResources.getDrawable(context, R.drawable.popup_bg);
        final Drawable selectedBackground = new InsetDrawable(
                AppCompatResources.getDrawable(context, R.drawable.selected_tab_background),
                (int) context.getResources().getDimension(R.dimen.tab_list_selected_inset_kitkat));
        boolean isZoomIn = status == AnimationStatus.SELECTED_CARD_ZOOM_IN
                || status == AnimationStatus.HOVERED_CARD_ZOOM_IN;
        boolean isHovered = status == AnimationStatus.HOVERED_CARD_ZOOM_IN
                || status == AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        boolean isRestore = status == AnimationStatus.CARD_RESTORE;
        long duration = isRestore ? RESTORE_ANIMATION_DURATION_MS
                                  : TabListRecyclerView.BASE_ANIMATION_DURATION_MS;
        float scale = isZoomIn ? ZOOM_IN_SCALE : 1f;
        ViewGroup.MarginLayoutParams backgroundParams =
                (ViewGroup.MarginLayoutParams) backgroundView.getLayoutParams();
        View animateView = isHovered ? contentView : view;

        if (status == AnimationStatus.HOVERED_CARD_ZOOM_IN) {
            backgroundParams.setMargins(
                    cardNormalMargin, cardNormalMargin, cardNormalMargin, cardNormalMargin);
            backgroundView.setBackground(greyBackground);
        }

        AnimatorSet scaleAnimator = new AnimatorSet();
        scaleAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (!isZoomIn) {
                    backgroundParams.setMargins(cardBackgroundMargin, cardBackgroundMargin,
                            cardBackgroundMargin, cardBackgroundMargin);
                    if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                        backgroundView.setBackground(
                                isSelected ? selectedBackground : normalBackground);
                    } else {
                        backgroundView.setBackground(normalBackground);
                    }
                }
                mIsAnimating = false;
            }
        });

        ObjectAnimator scaleX = ObjectAnimator.ofFloat(animateView, "scaleX", scale);
        ObjectAnimator scaleY = ObjectAnimator.ofFloat(animateView, "scaleY", scale);
        scaleX.setDuration(duration);
        scaleY.setDuration(duration);
        scaleAnimator.play(scaleX).with(scaleY);
        mIsAnimating = true;
        scaleAnimator.start();
    }

    @VisibleForTesting
    boolean getIsAnimatingForTesting() {
        return mIsAnimating;
    }
}
