// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_file_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/fileapi/file_system_types.h"

namespace content {

namespace {

// The CDM interface has a restriction that file names can not begin with _,
// so use it to prefix temporary files.
const char kTemporaryFilePrefix = '_';

// File size limit is 512KB. Licenses saved by the CDM are typically several
// hundreds of bytes.
const int64_t kMaxFileSizeBytes = 512 * 1024;

// Maximum length of a file name.
const size_t kFileNameMaxLength = 256;

std::string GetTempFileName(const std::string& file_name) {
  DCHECK(!base::StartsWith(file_name, std::string(1, kTemporaryFilePrefix),
                           base::CompareCase::SENSITIVE));
  return kTemporaryFilePrefix + file_name;
}

// The file system is different for each CDM and each origin. So track files
// in use based on (file system ID, origin, file name).
struct FileLockKey {
  FileLockKey(const std::string& file_system_id,
              const url::Origin& origin,
              const std::string& file_name)
      : file_system_id(file_system_id), origin(origin), file_name(file_name) {}
  ~FileLockKey() = default;

  // Allow use as a key in std::set.
  bool operator<(const FileLockKey& other) const {
    return std::tie(file_system_id, origin, file_name) <
           std::tie(other.file_system_id, other.origin, other.file_name);
  }

  std::string file_system_id;
  url::Origin origin;
  std::string file_name;
};

// File map shared by all CdmFileImpl objects to prevent read/write race.
// A lock must be acquired before opening a file to ensure that the file is not
// currently in use. The lock must be held until the file is closed.
class FileLockMap {
 public:
  FileLockMap() = default;
  ~FileLockMap() = default;

  // Acquire a lock on the file represented by |key|. Returns true if |key|
  // is not currently in use, false otherwise.
  bool AcquireFileLock(const FileLockKey& key) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Add a new entry. If |key| already has an entry, insert() tells so
    // with the second piece of the returned value and does not modify
    // the original.
    return file_lock_map_.insert(key).second;
  }

  // Release the lock held on the file represented by |key|.
  void ReleaseFileLock(const FileLockKey& key) {
    DVLOG(3) << __func__ << " file: " << key.file_name;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    auto entry = file_lock_map_.find(key);
    if (entry == file_lock_map_.end()) {
      NOTREACHED() << "Unable to release lock on file " << key.file_name;
      return;
    }

    file_lock_map_.erase(entry);
  }

 private:
  // Note that this map is never deleted. As entries are removed when a file
  // is closed, it should never get too large.
  std::set<FileLockKey> file_lock_map_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(FileLockMap);
};

// The FileLockMap is a global lock map shared by all CdmFileImpl instances.
FileLockMap* GetFileLockMap() {
  static auto* file_lock_map = new FileLockMap();
  return file_lock_map;
}

// Write |data| to |file|. Returns kSuccess if everything works, kFailure
// otherwise.  This method owns |file| and it will be closed at the end.
CdmFileImpl::Status WriteFile(base::File file, std::vector<uint8_t> data) {
  // As the temporary file should have been newly created, it should be empty.
  CHECK_EQ(0u, file.GetLength()) << "Temporary file is not empty.";
  int bytes_to_write = base::checked_cast<int>(data.size());

  TRACE_EVENT0("media", "CdmFileWriteFile");
  base::TimeTicks start = base::TimeTicks::Now();
  int bytes_written =
      file.Write(0, reinterpret_cast<const char*>(data.data()), bytes_to_write);
  base::TimeDelta write_time = base::TimeTicks::Now() - start;
  if (bytes_written != bytes_to_write) {
    DLOG(WARNING) << "Failed to write file. Requested " << bytes_to_write
                  << " bytes, wrote " << bytes_written;
    return CdmFileImpl::Status::kFailure;
  }

  // Only report writing time for successful writes.
  UMA_HISTOGRAM_TIMES("Media.EME.CdmFileIO.WriteTime", write_time);

  return CdmFileImpl::Status::kSuccess;
}

// File stream operations need an IOBuffer to hold the data. This class stores
// the data in a std::vector<uint8_t> to match what is used in the
// mojom::CdmFile API.
class CdmFileIOBuffer : public net::IOBuffer {
 public:
  // Create an empty buffer of size |size|.
  explicit CdmFileIOBuffer(size_t size) : buffer_(size) {
    data_ = reinterpret_cast<char*>(buffer_.data());
  }

  // Returns ownership of |buffer_| to the caller.
  std::vector<uint8_t>&& TakeData() { return std::move(buffer_); }

 protected:
  ~CdmFileIOBuffer() override { data_ = nullptr; }

 private:
  std::vector<uint8_t> buffer_;
};

}  // namespace

// Read a file using FileStreamReader. Implemented as a separate class so that
// it can be run on the IO thread.
class CdmFileImpl::FileReader {
 public:
  // Returns whether the read operation succeeded or not. If |result| = true,
  // then |data| is the contents of the file.
  using ReadDoneCB =
      base::OnceCallback<void(bool result, std::vector<uint8_t> data)>;

  FileReader() = default;

  // Reads the contents of |file_url| and calls |callback| with the result
  // (file contents on success, empty data on error).
  void Read(scoped_refptr<storage::FileSystemContext> file_system_context,
            const storage::FileSystemURL& file_url,
            ReadDoneCB callback) {
    DVLOG(3) << __func__ << " url: " << file_url.DebugString();
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!callback_);
    DCHECK(!file_stream_reader_);

    callback_ = std::move(callback);

    file_stream_reader_ = file_system_context->CreateFileStreamReader(
        file_url, 0, kMaxFileSizeBytes, base::Time());
    auto result = file_stream_reader_->GetLength(
        base::BindOnce(&FileReader::OnGetLength, weak_factory_.GetWeakPtr()));
    DVLOG(3) << __func__ << " GetLength(): " << result;

    // If GetLength() is running asynchronously, simply return.
    if (result == net::ERR_IO_PENDING)
      return;

    // GetLength() was synchronous, so pass the result on.
    OnGetLength(result);
  }

 private:
  // Called when the size of the file to be read is known. Allocates a buffer
  // large enough to hold the contents, then attempts to read the contents into
  // the buffer. |result| will be the length of the file (if >= 0) or a net::
  // error on failure (if < 0).
  void OnGetLength(int64_t result) {
    DVLOG(3) << __func__ << " result: " << result;
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(callback_);
    DCHECK(file_stream_reader_);

    // If the file doesn't exist, then pretend it is empty.
    if (result == net::ERR_FILE_NOT_FOUND) {
      std::move(callback_).Run(true, {});
      return;
    }

    // Any other failure is an error.
    if (result < 0) {
      DLOG(WARNING) << __func__
                    << " Unable to get file length. result = " << result;
      std::move(callback_).Run(false, {});
      return;
    }

    // Files are limited in size, so fail if file too big.
    if (result > kMaxFileSizeBytes) {
      DLOG(WARNING) << __func__
                    << " Too much data to read. #bytes = " << result;
      std::move(callback_).Run(false, {});
      return;
    }

    // Read() sizes (provided and returned) are type int, so cast appropriately.
    int bytes_to_read = base::checked_cast<int>(result);
    auto buffer = base::MakeRefCounted<CdmFileIOBuffer>(
        base::checked_cast<size_t>(bytes_to_read));

    // Read the contents of the file into |buffer|.
    base::TimeTicks start_time = base::TimeTicks::Now();
    result = file_stream_reader_->Read(
        buffer.get(), bytes_to_read,
        base::BindOnce(&FileReader::OnRead, weak_factory_.GetWeakPtr(), buffer,
                       start_time, bytes_to_read));
    DVLOG(3) << __func__ << " Read(): " << result;

    // If Read() is running asynchronously, simply return.
    if (result == net::ERR_IO_PENDING)
      return;

    // Read() was synchronous, so pass the result on.
    OnRead(std::move(buffer), start_time, bytes_to_read, result);
  }

  // Called when the file has been read and returns the result to the callback
  // provided to Read(). |result| will be the number of bytes read (if >= 0) or
  // a net:: error on failure (if < 0).
  void OnRead(scoped_refptr<CdmFileIOBuffer> buffer,
              base::TimeTicks start_time,
              int bytes_to_read,
              int result) {
    DVLOG(3) << __func__ << " Requested " << bytes_to_read << " bytes, got "
             << result;
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(callback_);
    DCHECK(file_stream_reader_);

    if (result != bytes_to_read) {
      // Unable to read the contents of the file completely.
      DLOG(WARNING) << "Failed to read file. Requested " << bytes_to_read
                    << " bytes, got " << result;
      std::move(callback_).Run(false, {});
      return;
    }

    // Only report reading time for successful reads.
    base::TimeDelta read_time = base::TimeTicks::Now() - start_time;
    UMA_HISTOGRAM_TIMES("Media.EME.CdmFileIO.ReadTime", read_time);

    // Successful read. Return the bytes read.
    std::move(callback_).Run(true, std::move(buffer->TakeData()));
  }

  // Called when the read operation is done.
  ReadDoneCB callback_;

  // Used to read the stream.
  std::unique_ptr<storage::FileStreamReader> file_stream_reader_;

  base::WeakPtrFactory<FileReader> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FileReader);
};

// static
bool CdmFileImpl::IsValidName(const std::string& name) {
  // File names must only contain letters (A-Za-z), digits(0-9), or "._-",
  // and not start with "_". It must contain at least 1 character, and not
  // more then |kFileNameMaxLength| characters.
  if (name.empty() || name.length() > kFileNameMaxLength ||
      name[0] == kTemporaryFilePrefix) {
    return false;
  }

  for (const auto ch : name) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '.' &&
        ch != '_' && ch != '-') {
      return false;
    }
  }

  return true;
}

CdmFileImpl::CdmFileImpl(
    const std::string& file_name,
    const url::Origin& origin,
    const std::string& file_system_id,
    const std::string& file_system_root_uri,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : file_name_(file_name),
      temp_file_name_(GetTempFileName(file_name_)),
      origin_(origin),
      file_system_id_(file_system_id),
      file_system_root_uri_(file_system_root_uri),
      file_system_context_(file_system_context) {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK(IsValidName(file_name_));
}

CdmFileImpl::~CdmFileImpl() {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (read_callback_)
    std::move(read_callback_).Run(Status::kFailure, {});

  if (file_reader_) {
    // |file_reader_| must be deleted on the IO thread.
    base::CreateSequencedTaskRunner({BrowserThread::IO})
        ->DeleteSoon(FROM_HERE, std::move(file_reader_));
  }

  if (file_locked_)
    ReleaseFileLock(file_name_);
}

bool CdmFileImpl::Initialize() {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!file_locked_);

  // Grab the lock on |file_name_|. The lock will be held until this object is
  // destructed.
  if (!AcquireFileLock(file_name_)) {
    DVLOG(2) << "File " << file_name_ << " is already in use.";
    return false;
  }

  // We have the lock on |file_name_|. |file_locked_| is set to simplify
  // validation, and to help destruction not have to check.
  file_locked_ = true;
  return true;
}

void CdmFileImpl::Read(ReadCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  // Save |callback| for later use.
  read_callback_ = std::move(callback);

  // As reading is done on the IO thread, when it's done ReadDone() needs to be
  // called back on this thread.
  auto read_done_cb = media::BindToCurrentLoop(
      base::BindOnce(&CdmFileImpl::ReadDone, weak_factory_.GetWeakPtr()));

  // Create the file reader that runs on the IO thread, and then call Read() on
  // the IO thread. Use of base::Unretained() is OK as the reader is owned by
  // |this|, and if |this| is destructed it will destroy the file reader on the
  // IO thread.
  file_reader_ = std::make_unique<FileReader>();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileReader::Read, base::Unretained(file_reader_.get()),
                     file_system_context_, CreateFileSystemURL(file_name_),
                     std::move(read_done_cb)));
}

void CdmFileImpl::ReadDone(bool success, std::vector<uint8_t> data) {
  DVLOG(3) << __func__ << " file: " << file_name_
           << ", success: " << (success ? "yes" : "no");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);
  DCHECK(file_reader_);
  DCHECK(read_callback_);

  // We are done with the reader, so destroy it.
  file_reader_.reset();

  if (!success) {
    // Unable to read the contents of the file.
    std::move(read_callback_).Run(Status::kFailure, {});
    return;
  }

  std::move(read_callback_).Run(Status::kSuccess, std::move(data));
}

void CdmFileImpl::OpenFile(const std::string& file_name,
                           uint32_t file_flags,
                           CreateOrOpenCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name << ", flags: " << file_flags;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  storage::FileSystemURL file_url = CreateFileSystemURL(file_name);
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());
  operation_context->set_allowed_bytes_growth(storage::QuotaManager::kNoLimit);
  DVLOG(3) << "Opening " << file_url.DebugString();

  file_util->CreateOrOpen(std::move(operation_context), file_url, file_flags,
                          std::move(callback));
}

void CdmFileImpl::Write(const std::vector<uint8_t>& data,
                        WriteCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name_ << ", size: " << data.size();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  // If there is no data to write, delete the file to save space.
  if (data.empty()) {
    DeleteFile(std::move(callback));
    return;
  }

  // Files are limited in size, so fail if file too big. This should have been
  // checked by the caller, but we don't fully trust IPC.
  if (data.size() > kMaxFileSizeBytes) {
    DLOG(WARNING) << __func__
                  << " Too much data to write. #bytes = " << data.size();
    std::move(callback).Run(Status::kFailure);
    return;
  }

  // Open the temporary file for writing. Specifying FLAG_CREATE_ALWAYS which
  // will overwrite any existing file.
  OpenFile(
      temp_file_name_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE,
      base::BindOnce(&CdmFileImpl::OnTempFileOpenedForWriting,
                     weak_factory_.GetWeakPtr(), data, std::move(callback)));
}

void CdmFileImpl::OnTempFileOpenedForWriting(
    std::vector<uint8_t> data,
    WriteCallback callback,
    base::File file,
    base::OnceClosure on_close_callback) {
  DVLOG(3) << __func__ << " file: " << temp_file_name_
           << ", bytes_to_write: " << data.size();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (!file.IsValid()) {
    DLOG(WARNING) << "Unable to open file " << temp_file_name_ << ", error: "
                  << base::File::ErrorToString(file.error_details());
    std::move(callback).Run(Status::kFailure);
    return;
  }

  // Writing to |file| must be done on a thread that allows blocking, so post a
  // task to do the writing on a separate thread. When that completes we need to
  // rename the file in order to replace any existing contents.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&WriteFile, std::move(file), std::move(data)),
      base::BindOnce(&CdmFileImpl::OnFileWritten, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileWritten(WriteCallback callback, Status status) {
  DVLOG(3) << __func__ << " file: " << temp_file_name_
           << ", status: " << status;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (status != Status::kSuccess) {
    // Write failed, so fail.
    std::move(callback).Run(status);
    return;
  }

  // Now rename |temp_file_name_| to |file_name_|.
  storage::FileSystemURL src_file_url = CreateFileSystemURL(temp_file_name_);
  storage::FileSystemURL dest_file_url = CreateFileSystemURL(file_name_);
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());
  DVLOG(3) << "Renaming " << src_file_url.DebugString() << " to "
           << dest_file_url.DebugString();
  file_util->MoveFileLocal(
      std::move(operation_context), src_file_url, dest_file_url,
      storage::FileSystemOperation::OPTION_NONE,
      base::BindOnce(&CdmFileImpl::OnFileRenamed, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileRenamed(WriteCallback callback,
                                base::File::Error move_result) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  // Was the rename successful?
  if (move_result != base::File::FILE_OK) {
    DLOG(WARNING) << "Unable to rename file " << temp_file_name_ << " to "
                  << file_name_
                  << ", error: " << base::File::ErrorToString(move_result);
    std::move(callback).Run(Status::kFailure);
    return;
  }

  std::move(callback).Run(Status::kSuccess);
}

void CdmFileImpl::DeleteFile(WriteCallback callback) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  storage::FileSystemURL file_url = CreateFileSystemURL(file_name_);
  storage::AsyncFileUtil* file_util = file_system_context_->GetAsyncFileUtil(
      storage::kFileSystemTypePluginPrivate);
  auto operation_context =
      std::make_unique<storage::FileSystemOperationContext>(
          file_system_context_.get());

  DVLOG(3) << "Deleting " << file_url.DebugString();
  file_util->DeleteFile(
      std::move(operation_context), file_url,
      base::BindOnce(&CdmFileImpl::OnFileDeleted, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CdmFileImpl::OnFileDeleted(WriteCallback callback,
                                base::File::Error result) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(file_locked_);

  if (result != base::File::FILE_OK &&
      result != base::File::FILE_ERROR_NOT_FOUND) {
    DLOG(WARNING) << "Unable to delete file " << file_name_
                  << ", error: " << base::File::ErrorToString(result);
    std::move(callback).Run(Status::kFailure);
    return;
  }

  std::move(callback).Run(Status::kSuccess);
}

storage::FileSystemURL CdmFileImpl::CreateFileSystemURL(
    const std::string& file_name) {
  return file_system_context_->CrackURL(
      GURL(file_system_root_uri_ + file_name));
}

bool CdmFileImpl::AcquireFileLock(const std::string& file_name) {
  FileLockKey file_lock_key(file_system_id_, origin_, file_name);
  return GetFileLockMap()->AcquireFileLock(file_lock_key);
}

void CdmFileImpl::ReleaseFileLock(const std::string& file_name) {
  FileLockKey file_lock_key(file_system_id_, origin_, file_name);
  GetFileLockMap()->ReleaseFileLock(file_lock_key);
}

}  // namespace content
