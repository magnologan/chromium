// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.ParcelFileDescriptor;
import android.support.annotation.VisibleForTesting;

import org.chromium.android_webview.VariationsUtils;
import org.chromium.base.Log;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.text.ParseException;
import java.util.Date;

/**
 * VariationsSeedHolder is a singleton which manages the local copy of the variations seed - both
 * the file and the SeedInfo object containing the data loaded from that file - in WebView's
 * variations service process. VariationsSeedHolder is used by VariationsSeedServer (to serve the
 * seed to apps) and AwVariationsSeedFetcher (to update the seed). VariationsSeedHolder guards
 * concurrent access to the seed by serializing all operations onto mSeedThread.
 * VariationsSeedHolder is not meant to be used outside the variations service.
 */
@VisibleForTesting
public class VariationsSeedHolder {
    private static final String TAG = "VariationsSeedHolder";

    private static final VariationsSeedHolder sInstance = new VariationsSeedHolder();

    private static void writeSeedWithoutClosing(SeedInfo seed, ParcelFileDescriptor destination) {
        // writeSeed() will close "out", but closing "out" will not close "destination".
        FileOutputStream out = new FileOutputStream(destination.getFileDescriptor());
        VariationsUtils.writeSeed(out, seed);
    }

    // Use mSeedHandler to send tasks to mSeedThread.
    private final HandlerThread mSeedThread;
    private final Handler mSeedHandler;
    // mSeed is the service's copy of the seed. It should only be used on mSeedThread.
    private SeedInfo mSeed;
    // Set true when we fail to load a seed, to prevent future loads until SeedUpdater runs.
    private boolean mFailedReadingSeed;

    // A Runnable which handles an individual request for the seed. Must run on mSeedThread.
    private class SeedWriter implements Runnable {
        private ParcelFileDescriptor mDestination;

        // mDestinationDate is the date field of the requester's current seed, in milliseconds since
        // epoch, or Long.MIN_VALUE if the requester has no seed. Only write our seed if our seed is
        // newer than mDestinationDate.
        private long mDestinationDate;

        public SeedWriter(ParcelFileDescriptor destination, long date) {
            mDestination = destination;
            mDestinationDate = date;
        }

        @Override
        public void run() {
            assert Thread.currentThread() == mSeedThread;

            try {
                scheduleFetchIfNeeded();

                // Load the seed if necessary.
                if (VariationsSeedHolder.this.mSeed == null && !mFailedReadingSeed) {
                    VariationsSeedHolder.this.mSeed =
                            VariationsUtils.readSeedFile(VariationsUtils.getSeedFile());
                    mFailedReadingSeed = VariationsSeedHolder.this.mSeed == null;
                }

                // If there's no seed available, the app will have to request again.
                if (VariationsSeedHolder.this.mSeed == null) return;

                Date loadedSeedDate;
                try {
                    loadedSeedDate = VariationsSeedHolder.this.mSeed.parseDate();
                } catch (ParseException e) {
                    // Should never happen, as date was alread verified by readSeedFile.
                    assert false;
                    return;
                }

                if (mDestinationDate < loadedSeedDate.getTime()) {
                    writeSeedWithoutClosing(VariationsSeedHolder.this.mSeed, mDestination);
                }
            } finally {
                VariationsUtils.closeSafely(mDestination);
                onWriteFinished();
            }
        }
    }

    // A Runnable which updates both mSeed and the service's seed file. Must run on mSeedThread.
    private class SeedUpdater implements Runnable {
        private SeedInfo mNewSeed;
        private Runnable mOnFinished;

        public SeedUpdater(SeedInfo newSeed, Runnable onFinished) {
            mNewSeed = newSeed;
            mOnFinished = onFinished;
        }

        @Override
        public void run() {
            assert Thread.currentThread() == mSeedThread;
            try {
                VariationsSeedHolder.this.mSeed = mNewSeed;

                // Update the seed file.
                File newSeedFile = VariationsUtils.getNewSeedFile();
                FileOutputStream out;
                try {
                    out = new FileOutputStream(newSeedFile);
                } catch (FileNotFoundException e) {
                    Log.e(TAG, "Failed to open seed file " + newSeedFile + " for update");
                    return;
                }
                if (!VariationsUtils.writeSeed(out, VariationsSeedHolder.this.mSeed)) {
                    Log.e(TAG, "Failed to write seed file " + newSeedFile + " for update");
                    return;
                }
                VariationsUtils.replaceOldWithNewSeed();
                mFailedReadingSeed = false;
            } finally {
                mOnFinished.run();
            }
        }
    }

    @VisibleForTesting
    protected VariationsSeedHolder() {
        mSeedThread = new HandlerThread(/*name=*/"seed_holder");
        mSeedThread.start();
        mSeedHandler = new Handler(mSeedThread.getLooper());
    }

    /* package */ static VariationsSeedHolder getInstance() {
        return sInstance;
    }

    @VisibleForTesting
    public void writeSeedIfNewer(ParcelFileDescriptor destination, long date) {
        mSeedHandler.post(new SeedWriter(destination, date));
    }

    @VisibleForTesting
    public void updateSeed(SeedInfo newSeed, Runnable onFinished) {
        mSeedHandler.post(new SeedUpdater(newSeed, onFinished));
    }

    @VisibleForTesting
    public void scheduleFetchIfNeeded() {
        AwVariationsSeedFetcher.scheduleIfNeeded();
    }

    // overridden by tests
    public void onWriteFinished() {}
}
