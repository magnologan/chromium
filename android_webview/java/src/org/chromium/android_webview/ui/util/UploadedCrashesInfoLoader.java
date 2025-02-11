// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.ui.util;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Parses upload log file in crash directory where crash upload id and time are written.
 */
public class UploadedCrashesInfoLoader extends CrashInfoLoader {
    private File mLogFile;

    /**
     * @param logsFile upload log file to parse.
     */
    public UploadedCrashesInfoLoader(File logFile) {
        mLogFile = logFile;
    }

    /**
     * Parse and load crashes upload info from upload log file.
     *
     * @return list of crashes info.
     */
    @Override
    public List<CrashInfo> loadCrashesInfo() {
        List<CrashInfo> uploads = new ArrayList<>();

        if (mLogFile.exists()) {
            try {
                BufferedReader reader = new BufferedReader(new FileReader(mLogFile));
                String line = reader.readLine();
                while (line != null) {
                    CrashInfo info = parseLogEntry(line);
                    if (info != null) {
                        uploads.add(info);
                    }
                    line = reader.readLine();
                }
                reader.close();
            } catch (IOException e) {
            }
        }

        return uploads;
    }

    private CrashInfo parseLogEntry(String logEntry) {
        // uploads log entry are on the format:
        // <upload-time>,<upload-id>,<crash-local-id>
        String[] components = logEntry.split(",");
        // Skip any blank (or corrupted) lines or that have missing info.
        if (components.length != 3 || components[0].isEmpty() || components[1].isEmpty()
                || components[2].isEmpty()) {
            return null;
        }

        CrashInfo info = new CrashInfo();
        info.uploadState = UploadState.UPLOADED;
        try {
            info.uploadTime = Long.parseLong(components[0]);
        } catch (NumberFormatException e) {
            return null;
        }
        info.uploadId = components[1];
        info.localId = components[2];

        return info;
    }
}
