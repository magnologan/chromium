// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Uses android_dlopen_ext() to share relocations.

// This source code *cannot* depend on anything from base/ or the C++
// STL, to keep the final library small, and avoid ugly dependency issues.

#include "modern_linker_jni.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <limits.h>
#include <link.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android/dlext.h>
#include "linker_jni.h"

// Not defined on all platforms. As this linker is only supported on ARM32/64,
// x86/x86_64 and MIPS, page size is always 4k.
#if !defined(PAGE_SIZE)
#define PAGE_SIZE (1 << 12)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#endif

#define PAGE_START(x) ((x)&PAGE_MASK)
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE - 1))

namespace chromium_android_linker {
namespace {

// Record of the Java VM passed to JNI_OnLoad().
static JavaVM* s_java_vm = nullptr;

// Convenience wrapper around dlsym() on the main executable. Returns
// the address of the requested symbol, or nullptr if not found. Status
// is available from dlerror().
void* Dlsym(const char* symbol_name) {
  static void* handle = nullptr;

  if (!handle)
    handle = dlopen(nullptr, RTLD_NOW);

  void* result = dlsym(handle, symbol_name);
  return result;
}

// dl_iterate_phdr() wrapper, accessed via dlsym lookup. Done this way.
// so that this code compiles for Android versions that are too early to
// offer it. Checks in LibraryLoader.java should ensure that we
// never reach here at runtime on Android versions that are too old to
// supply dl_iterate_phdr; that is, earlier than Android M. Returns
// false if no dl_iterate_phdr() is available, otherwise true with the
// return value from dl_iterate_phdr() in |status|.
bool DlIteratePhdr(int (*callback)(dl_phdr_info*, size_t, void*),
                   void* data,
                   int* status) {
  using DlIteratePhdrCallback = int (*)(dl_phdr_info*, size_t, void*);
  using DlIteratePhdrFunctionPtr = int (*)(DlIteratePhdrCallback, void*);
  static DlIteratePhdrFunctionPtr function_ptr = nullptr;

  if (!function_ptr) {
    function_ptr =
        reinterpret_cast<DlIteratePhdrFunctionPtr>(Dlsym("dl_iterate_phdr"));
    if (!function_ptr) {
      LOG_ERROR("dlsym: dl_iterate_phdr: %s", dlerror());
      return false;
    }
  }

  *status = (*function_ptr)(callback, data);
  return true;
}

// Convenience struct wrapper around android_dlextinfo.
struct AndroidDlextinfo {
  AndroidDlextinfo(int flags,
                   void* reserved_addr,
                   size_t reserved_size,
                   int relro_fd) {
    memset(&extinfo, 0, sizeof(extinfo));
    extinfo.flags = flags;
    extinfo.reserved_addr = reserved_addr;
    extinfo.reserved_size = reserved_size;
    extinfo.relro_fd = relro_fd;
  }

  android_dlextinfo extinfo;
};

// android_dlopen_ext() wrapper, accessed via dlsym lookup. Returns false
// if no android_dlopen_ext() is available, otherwise true with the return
// value from android_dlopen_ext() in |status|.
bool AndroidDlopenExt(const char* filename,
                      int flag,
                      const AndroidDlextinfo* dlextinfo,
                      void** status) {
  using DlopenExtFunctionPtr =
      void* (*)(const char*, int, const android_dlextinfo*);
  static DlopenExtFunctionPtr function_ptr = nullptr;

  if (!function_ptr) {
    function_ptr =
        reinterpret_cast<DlopenExtFunctionPtr>(Dlsym("android_dlopen_ext"));
    if (!function_ptr) {
      LOG_ERROR("dlsym: android_dlopen_ext: %s", dlerror());
      return false;
    }
  }

  android_dlextinfo ext = dlextinfo->extinfo;
  LOG_INFO(
      "android_dlopen_ext:"
      " flags=0x%llx, reserved_addr=%p, reserved_size=%d, relro_fd=%d",
      static_cast<long long>(ext.flags), ext.reserved_addr,
      static_cast<int>(ext.reserved_size), ext.relro_fd);

  *status = (*function_ptr)(filename, flag, &ext);
  return true;
}

// Callback data for FindLoadedLibrarySize().
struct CallbackData {
  explicit CallbackData(void* address)
      : load_address(address), load_size(0), min_vaddr(0) {}

  const void* load_address;
  size_t load_size;
  size_t min_vaddr;
};

// Callback for dl_iterate_phdr(). Read phdrs to identify whether or not
// this library's load address matches the |load_address| passed in
// |data|. If yes, pass back load size and min vaddr via |data|. A non-zero
// return value terminates iteration.
int FindLoadedLibrarySize(dl_phdr_info* info, size_t size UNUSED, void* data) {
  CallbackData* callback_data = reinterpret_cast<CallbackData*>(data);

  // Use max and min vaddr to compute the library's load size.
  ElfW(Addr) min_vaddr = ~0;
  ElfW(Addr) max_vaddr = 0;

  bool is_matching = false;
  for (size_t i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
    if (phdr->p_type != PT_LOAD)
      continue;

    // See if this segment's load address matches what we passed to
    // android_dlopen_ext as extinfo.reserved_addr.
    void* load_addr = reinterpret_cast<void*>(info->dlpi_addr + phdr->p_vaddr);
    if (load_addr == callback_data->load_address)
      is_matching = true;

    if (phdr->p_vaddr < min_vaddr)
      min_vaddr = phdr->p_vaddr;
    if (phdr->p_vaddr + phdr->p_memsz > max_vaddr)
      max_vaddr = phdr->p_vaddr + phdr->p_memsz;
  }

  // If this library matches what we seek, return its load size.
  if (is_matching) {
    int page_size = sysconf(_SC_PAGESIZE);
    if (page_size != PAGE_SIZE)
      abort();

    callback_data->load_size = PAGE_END(max_vaddr) - PAGE_START(min_vaddr);
    callback_data->min_vaddr = min_vaddr;
    return true;
  }

  return false;
}

// Helper class for anonymous memory mapping.
class ScopedAnonymousMmap {
 public:
  static ScopedAnonymousMmap ReserveAtAddress(void* address, size_t size);

  ~ScopedAnonymousMmap() {
    if (addr_ && owned_)
      munmap(addr_, size_);
  }

  ScopedAnonymousMmap(ScopedAnonymousMmap&& o) {
    addr_ = o.addr_;
    size_ = o.size_;
    owned_ = o.owned_;
    o.Release();
  }

  void* address() const { return addr_; }
  size_t size() const { return size_; }
  void Release() { owned_ = false; }

 private:
  ScopedAnonymousMmap() = default;
  ScopedAnonymousMmap(void* addr, size_t size) : addr_(addr), size_(size) {}

 private:
  bool owned_ = true;
  void* addr_ = nullptr;
  size_t size_ = 0;

  // Move only.
  ScopedAnonymousMmap(const ScopedAnonymousMmap&) = delete;
  ScopedAnonymousMmap& operator=(const ScopedAnonymousMmap&) = delete;
};

// Makes sure the file descriptor is closed unless |Release()| is called.
class ScopedFileDescriptor {
 public:
  ScopedFileDescriptor(int fd) : fd_(fd), owned_(true) {}
  ~ScopedFileDescriptor() {
    if (owned_)
      Close();
  }
  ScopedFileDescriptor(ScopedFileDescriptor&& o)
      : fd_(o.fd_), owned_(o.owned_) {
    o.owned_ = false;
  }
  int get() const { return fd_; }
  void Release() { owned_ = false; }
  void Close() {
    if (fd_ != -1)
      close(fd_);
    owned_ = false;
  }

 private:
  const int fd_;
  bool owned_;

  // Move only.
  ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
  ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;
};

// Reserves an address space range, starting at |address|.
// If successful, returns a valid mapping, otherwise returns an empty one.
ScopedAnonymousMmap ScopedAnonymousMmap::ReserveAtAddress(void* address,
                                                          size_t size) {
  void* actual_address =
      mmap(address, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (actual_address == MAP_FAILED) {
    LOG_INFO("mmap failed: %s", strerror(errno));
    return {};
  }

  if (actual_address && actual_address != address) {
    LOG_ERROR("Failed to obtain fixed address for load");
    return {};
  }

  return {actual_address, size};
}

// Returns the actual size of the library loaded at |addr| in |load_size|, and
// the min vaddr in |min_vaddr|. Returns false if the library appears not to be
// loaded.
bool GetLibraryLoadSize(void* addr, size_t* load_size, size_t* min_vaddr) {
  LOG_INFO("Called for %p", addr);

  // Find the real load size and min vaddr for the library loaded at |addr|.
  CallbackData callback_data(addr);
  int status = 0;
  if (!DlIteratePhdr(&FindLoadedLibrarySize, &callback_data, &status)) {
    LOG_ERROR("No dl_iterate_phdr function found");
    return false;
  }
  if (!status) {
    LOG_ERROR("Failed to find library at address %p", addr);
    return false;
  }

  *load_size = callback_data.load_size;
  *min_vaddr = callback_data.min_vaddr;
  return true;
}

// Reopens |fd| that was initially opened from |path| as a read-only fd.
// Deletes the file in the process, and returns the new read only file
// descriptor in case of success, -1 otherwise.
ScopedFileDescriptor ReopenReadOnly(const String& path,
                                    ScopedFileDescriptor original_fd) {
  const char* filepath = path.c_str();
  original_fd.Close();
  ScopedFileDescriptor scoped_fd{open(filepath, O_RDONLY)};
  if (scoped_fd.get() == -1) {
    LOG_ERROR("open: %s: %s", path.c_str(), strerror(errno));
    return -1;
  }

  // Delete the directory entry for the RELRO file. The fd we hold ensures
  // that its data remains intact.
  if (unlink(filepath) == -1) {
    LOG_ERROR("unlink: %s: %s", filepath, strerror(errno));
    return -1;
  }
  return scoped_fd;
}

// Resizes the address space reservation to the actual required size.
// Failure here is only a warning, as at worst this wastes virtual address
// space, not actual memory.
void ResizeMapping(const ScopedAnonymousMmap& mapping) {
  // After loading we can find the actual size of the library. It should
  // be less than the space we reserved for it.
  size_t load_size = 0;
  size_t min_vaddr = 0;
  if (!GetLibraryLoadSize(mapping.address(), &load_size, &min_vaddr)) {
    LOG_ERROR("Unable to find size for load at %p", mapping.address());
    return;
  }

  // Trim the reservation mapping to match the library's actual size. Failure
  // to resize is not a fatal error. At worst we lose a portion of virtual
  // address space that we might otherwise have recovered. Note that trimming
  // the mapping here requires that we have already released the scoped
  // mapping.
  const uintptr_t uintptr_addr = reinterpret_cast<uintptr_t>(mapping.address());
  if (mapping.size() > load_size) {
    // Unmap the part of the reserved address space that is beyond the end of
    // the loaded library data.
    void* unmap = reinterpret_cast<void*>(uintptr_addr + load_size);
    const size_t length = mapping.size() - load_size;
    if (munmap(unmap, length) == -1) {
      LOG_ERROR("WARNING: unmap of %d bytes at %p failed: %s",
                static_cast<int>(length), unmap, strerror(errno));
    }
  } else {
    LOG_ERROR("WARNING: library reservation was too small");
  }
}

// Calls JNI_OnLoad() in the library referenced by |handle|.
// Returns true for success.
bool CallJniOnLoad(void* handle) {
  // Locate and if found then call the loaded library's JNI_OnLoad() function.
  using JNI_OnLoadFunctionPtr = int (*)(void* vm, void* reserved);
  auto jni_onload =
      reinterpret_cast<JNI_OnLoadFunctionPtr>(dlsym(handle, "JNI_OnLoad"));
  if (jni_onload != nullptr) {
    // Check that JNI_OnLoad returns a usable JNI version.
    int jni_version = (*jni_onload)(s_java_vm, nullptr);
    if (jni_version < JNI_VERSION_1_4) {
      LOG_ERROR("JNI version is invalid: %d", jni_version);
      return false;
    }
  }
  return true;
}

// Load the library at |path| at address |wanted_address| if possible, and
// creates a file with relro at |relocations_path|.
//
// In case of success, returns a readonly file descriptor to the relocations,
// otherwise returns -1.
int LoadCreateSharedRelocations(const String& path,
                                void* wanted_address,
                                const String& relocations_path) {
  LOG_INFO("Entering");
  ScopedAnonymousMmap mapping = ScopedAnonymousMmap::ReserveAtAddress(
      wanted_address, kAddressSpaceReservationSize);
  if (!mapping.address())
    return -1;

  unlink(relocations_path.c_str());
  ScopedFileDescriptor relro_fd = ScopedFileDescriptor{open(
      relocations_path.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)};
  if (relro_fd.get() == -1) {
    LOG_ERROR("open: %s: %s", relocations_path.c_str(), strerror(errno));
    return -1;
  }

  int flags = ANDROID_DLEXT_RESERVED_ADDRESS | ANDROID_DLEXT_WRITE_RELRO;
  AndroidDlextinfo dlextinfo(flags, mapping.address(), mapping.size(),
                             relro_fd.get());
  void* handle = nullptr;
  if (!AndroidDlopenExt(path.c_str(), RTLD_NOW, &dlextinfo, &handle)) {
    LOG_ERROR("No android_dlopen_ext function found");
    return -1;
  }
  if (handle == nullptr) {
    LOG_ERROR("android_dlopen_ext: %s", dlerror());
    return -1;
  }

  mapping.Release();
  ResizeMapping(mapping);
  if (!CallJniOnLoad(handle)) {
    unlink(relocations_path.c_str());
    return false;
  }
  ScopedFileDescriptor scoped_fd =
      ReopenReadOnly(relocations_path, std::move(relro_fd));
  scoped_fd.Release();
  return scoped_fd.get();
}

// Load the library at |path| at address |wanted_address| if possible, and
// uses the relocations in |relocations_fd| if possible.
bool LoadUseSharedRelocations(const String& path,
                              void* wanted_address,
                              int relocations_fd) {
  LOG_INFO("Entering");
  ScopedAnonymousMmap mapping = ScopedAnonymousMmap::ReserveAtAddress(
      wanted_address, kAddressSpaceReservationSize);
  if (!mapping.address())
    return false;

  int flags = ANDROID_DLEXT_RESERVED_ADDRESS | ANDROID_DLEXT_USE_RELRO;
  AndroidDlextinfo dlextinfo(flags, mapping.address(), mapping.size(),
                             relocations_fd);
  void* handle = nullptr;
  if (!AndroidDlopenExt(path.c_str(), RTLD_NOW, &dlextinfo, &handle)) {
    LOG_ERROR("No android_dlopen_ext function found");
    return false;
  }
  if (handle == nullptr) {
    LOG_ERROR("android_dlopen_ext: %s", dlerror());
    return false;
  }

  mapping.Release();
  ResizeMapping(mapping);
  if (!CallJniOnLoad(handle))
    return false;

  return true;
}

}  // namespace

// Get the CPU ABI string for which the linker is running.
//
// The returned string is used to construct the path to libchrome.so when
// loading directly from APK.
//
// |env| is the current JNI environment handle.
// |clazz| is the static class handle for org.chromium.base.Linker,
// and is ignored here.
// Returns the CPU ABI string for which the linker is running.
JNI_GENERATOR_EXPORT jstring
Java_org_chromium_base_library_1loader_ModernLinker_nativeGetCpuAbi(
    JNIEnv* env,
    jclass clazz) {
  return env->NewStringUTF(CURRENT_ABI);
}

JNI_GENERATOR_EXPORT jboolean
Java_org_chromium_base_library_1loader_ModernLinker_nativeLoadLibraryCreateRelros(
    JNIEnv* env,
    jclass clazz,
    jstring jdlopen_ext_path,
    jlong load_address,
    jstring jrelro_path,
    jobject lib_info_obj) {
  LOG_INFO("Entering");

  String library_path(env, jdlopen_ext_path);
  String relro_path(env, jrelro_path);

  if (!IsValidAddress(load_address)) {
    LOG_ERROR("Invalid address 0x%llx", static_cast<long long>(load_address));
    return false;
  }
  void* address = reinterpret_cast<void*>(load_address);

  int fd = LoadCreateSharedRelocations(library_path, address, relro_path);
  if (fd == -1)
    return false;

  // Note the shared RELRO fd in the supplied libinfo object. In this
  // implementation the RELRO start is set to the library's load address,
  // and the RELRO size is unused.
  const size_t cast_addr = reinterpret_cast<size_t>(address);
  s_lib_info_fields.SetRelroInfo(env, lib_info_obj, cast_addr, 0, fd);

  return true;
}

JNI_GENERATOR_EXPORT jboolean
Java_org_chromium_base_library_1loader_ModernLinker_nativeLoadLibraryUseRelros(
    JNIEnv* env,
    jclass clazz,
    jstring jdlopen_ext_path,
    jlong load_address,
    jint relro_fd) {
  LOG_INFO("Entering");

  String library_path(env, jdlopen_ext_path);

  if (!IsValidAddress(load_address)) {
    LOG_ERROR("Invalid address 0x%llx", static_cast<long long>(load_address));
    return false;
  }
  void* address = reinterpret_cast<void*>(load_address);

  return LoadUseSharedRelocations(library_path, address, relro_fd);
}

bool ModernLinkerJNIInit(JavaVM* vm, JNIEnv* env) {
  s_java_vm = vm;
  return true;
}

}  // namespace chromium_android_linker
