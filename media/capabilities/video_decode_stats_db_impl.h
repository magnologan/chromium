// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_VIDEO_DECODE_STATS_DB_IMPL_H_
#define MEDIA_CAPABILITIES_VIDEO_DECODE_STATS_DB_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/capabilities/video_decode_stats_db.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
class Clock;
}  // namespace base

namespace media {

class DecodeStatsProto;

// LevelDB implementation of VideoDecodeStatsDB. This class is not
// thread safe. All API calls should happen on the same sequence used for
// construction. API callbacks will also occur on this sequence.
class MEDIA_EXPORT VideoDecodeStatsDBImpl : public VideoDecodeStatsDB {
 public:
  static const char kMaxFramesPerBufferParamName[];
  static const char kMaxDaysToKeepStatsParamName[];
  static const char kEnableUnweightedEntriesParamName[];

  // Create an instance! |db_dir| specifies where to store LevelDB files to
  // disk. LevelDB generates a handful of files, so its recommended to provide a
  // dedicated directory to keep them isolated.
  static std::unique_ptr<VideoDecodeStatsDBImpl> Create(base::FilePath db_dir);

  ~VideoDecodeStatsDBImpl() override;

  // Implement VideoDecodeStatsDB.
  void Initialize(InitializeCB init_cb) override;
  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& entry,
                         AppendDecodeStatsCB append_done_cb) override;
  void GetDecodeStats(const VideoDescKey& key,
                      GetDecodeStatsCB get_stats_cb) override;
  void ClearStats(base::OnceClosure clear_done_cb) override;

 private:
  friend class VideoDecodeStatsDBImplTest;

  // Private constructor only called by tests (friends). Production code
  // should always use the static Create() method.
  VideoDecodeStatsDBImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db,
      const base::FilePath& dir);

  // Default |last_write_time| for DB entries that lack a time stamp due to
  // using an earlier version of DecodeStatsProto. Date chosen so old stats from
  // previous version will expire (unless new stats arrive) roughly 2 months
  // after the proto update hits the chrome Stable channel (M71).
  static constexpr char kDefaultWriteTime[] = "01-FEB-2019 12:00pm";

  // Number of decoded frames to keep in the rolling "window" for a given entry
  // in the database.
  static int GetMaxFramesPerBuffer();

  // Number of days after which stats will be discarded if not updated. This
  // avoids users getting stuck with a bad capability prediction that may have
  // been due to one-off circumstances.
  static int GetMaxDaysToKeepStats();

  // When true, each playback entry in the DB should be given equal weight
  // regardless of how many frames were decoded.
  static bool GetEnableUnweightedEntries();

  // Called when the database has been initialized. Will immediately call
  // |init_cb| to forward |success|.
  void OnInit(InitializeCB init_cb, bool success);

  // Returns true if the DB is successfully initialized.
  bool IsInitialized();

  // Passed as the callback for |OnGotDecodeStats| by |AppendDecodeStats| to
  // update the database once we've read the existing stats entry.
  void WriteUpdatedEntry(const VideoDescKey& key,
                         const DecodeStatsEntry& entry,
                         AppendDecodeStatsCB append_done_cb,
                         bool read_success,
                         std::unique_ptr<DecodeStatsProto> stats_proto);

  // Called when the database has been modified after a call to
  // |WriteUpdatedEntry|. Will run |append_done_cb| when done.
  void OnEntryUpdated(AppendDecodeStatsCB append_done_cb, bool success);

  // Called when GetDecodeStats() operation was performed. |get_stats_cb|
  // will be run with |success| and a |DecodeStatsEntry| created from
  // |stats_proto| or nullptr if no entry was found for the requested key.
  void OnGotDecodeStats(GetDecodeStatsCB get_stats_cb,
                        bool success,
                        std::unique_ptr<DecodeStatsProto> stats_proto);

  // Internal callback for first step of ClearStats(). Will clear all stats If
  // |keys| fetched successfully.
  void OnLoadAllKeysForClearing(base::OnceClosure clear_done_cb,
                                bool success,
                                std::unique_ptr<std::vector<std::string>> keys);

  // Internal callback for OnLoadAllKeysForClearing(), initially triggered by
  // ClearStats(). Method simply logs |success| and runs |clear_done_cb|.
  void OnStatsCleared(base::OnceClosure clear_done_cb, bool success);

  // Return true if:
  //     "now" - stats_proto.last_write_date > GeMaxDaysToKeepStats()
  bool AreStatsExpired(const DecodeStatsProto* const stats_proto);

  void set_wall_clock_for_test(const base::Clock* tick_clock) {
    wall_clock_ = tick_clock;
  }

  // Indicates whether initialization is completed. Does not indicate whether it
  // was successful. Will be reset upon calling DestroyStats(). Failed
  // initialization is signaled by setting |db_| to null.
  bool db_init_ = false;

  // ProtoDatabase instance. Set to nullptr if fatal database error is
  // encountered.
  std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db_;

  // Directory where levelDB should store database files.
  base::FilePath db_dir_;

  // For getting wall-clock time. Tests may override via SetClockForTest().
  const base::Clock* wall_clock_ = nullptr;

  // Stores parsed value of |kDefaultWriteTime|.
  base::Time default_write_time_;

  // Ensures all access to class members come on the same sequence. API calls
  // and callbacks should occur on the same sequence used during construction.
  // LevelDB operations happen on a separate task runner, but all LevelDB
  // callbacks to this happen on the checked sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoDecodeStatsDBImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsDBImpl);
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_VIDEO_DECODE_STATS_DB_IMPL_H_
