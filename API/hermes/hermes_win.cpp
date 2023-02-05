/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes_win.h"
#include "hermes/VM/Runtime.h"
#include "llvh/Support/raw_os_ostream.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <werapi.h>

#define CHECKED_RUNTIME(runtime) \
  (runtime == nullptr)           \
      ? hermes_error             \
      : reinterpret_cast<facebook::hermes::RuntimeWrapper *>(runtime)

#define CHECK_ARG(arg)   \
  if (arg == nullptr) {  \
    return hermes_error; \
  }

namespace facebook::hermes {

// Forward declaration
extern ::hermes::vm::Runtime &getVMRuntime(HermesRuntime &runtime) noexcept;

class CrashManagerImpl : public ::hermes::vm::CrashManager {
 public:
  void registerMemory(void *mem, size_t length) override {
    if (length >
        WER_MAX_MEM_BLOCK_SIZE) { // Hermes thinks we should save the whole
                                  // block, but WER allows 64K max
      _largeMemBlocks[(intptr_t)mem] = length;

      auto pieceCount = length / WER_MAX_MEM_BLOCK_SIZE;
      for (auto i = 0; i < pieceCount; i++) {
        WerRegisterMemoryBlock(
            (char *)mem + i * WER_MAX_MEM_BLOCK_SIZE, WER_MAX_MEM_BLOCK_SIZE);
      }

      WerRegisterMemoryBlock(
          (char *)mem + pieceCount * WER_MAX_MEM_BLOCK_SIZE,
          length - pieceCount * WER_MAX_MEM_BLOCK_SIZE);
    } else {
      WerRegisterMemoryBlock(mem, static_cast<DWORD>(length));
    }
  }

  void unregisterMemory(void *mem) override {
    if (_largeMemBlocks.find((intptr_t)mem) != _largeMemBlocks.end()) {
      // This memory was larger than what WER supports so we split it up into
      // chunks of size WER_MAX_MEM_BLOCK_SIZE
      auto pieceCount = _largeMemBlocks[(intptr_t)mem] / WER_MAX_MEM_BLOCK_SIZE;
      for (auto i = 0; i < pieceCount; i++) {
        WerUnregisterMemoryBlock((char *)mem + i * WER_MAX_MEM_BLOCK_SIZE);
      }

      WerUnregisterMemoryBlock(
          (char *)mem + pieceCount * WER_MAX_MEM_BLOCK_SIZE);

      _largeMemBlocks.erase((intptr_t)mem);
    } else {
      WerUnregisterMemoryBlock(mem);
    }
  }

  void setCustomData(const char *key, const char *val) override {
    auto strKey = Utf8ToUtf16(key);
    auto strValue = Utf8ToUtf16(val);
    WerRegisterCustomMetadata(strKey.c_str(), strValue.c_str());
  }

  void removeCustomData(const char *key) override {
    auto strKey = Utf8ToUtf16(key);
    WerUnregisterCustomMetadata(strKey.c_str());
  }

  void setContextualCustomData(const char *key, const char *val) override {
    std::wstringstream sstream;
    sstream << "TID" << std::this_thread::get_id() << Utf8ToUtf16(key);

    auto strKey = sstream.str();
    // WER expects valid XML element names, Hermes embeds ':' characters that
    // need to be replaced
    std::replace(strKey.begin(), strKey.end(), L':', L'_');

    auto strValue = Utf8ToUtf16(val);
    WerRegisterCustomMetadata(strKey.c_str(), strValue.c_str());
  }

  void removeContextualCustomData(const char *key) override {
    std::wstringstream sstream;
    sstream << "TID" << std::this_thread::get_id() << Utf8ToUtf16(key);

    auto strKey = sstream.str();
    // WER expects valid XML element names, Hermes embeds ':' characters that
    // need to be replaced
    std::replace(strKey.begin(), strKey.end(), L':', L'_');

    WerUnregisterCustomMetadata(strKey.c_str());
  }

  CallbackKey registerCallback(CallbackFunc cb) override {
    CallbackKey key = static_cast<CallbackKey>((intptr_t)std::addressof(cb));
    _callbacks.insert({key, std::move(cb)});
    return key;
  }

  void unregisterCallback(CallbackKey key) override {
    _callbacks.erase(static_cast<size_t>(key));
  }

  void setHeapInfo(const HeapInformation &heapInfo) override {
    _lastHeapInformation = heapInfo;
  }

  void crashHandler(int fd) const noexcept {
    for (const auto &cb : _callbacks) {
      cb.second(fd);
    }
  }

 private:
  std::wstring Utf8ToUtf16(const char *s) {
    size_t strLength = strnlen_s(
        s, 64); // 64 is maximum key length for WerRegisterCustomMetadata
    size_t requiredSize = 0;

    if (strLength != 0) {
      mbstowcs_s(&requiredSize, nullptr, 0, s, strLength);

      if (requiredSize != 0) {
        std::wstring buffer;
        buffer.resize(requiredSize + sizeof(wchar_t));

        if (mbstowcs_s(&requiredSize, &buffer[0], requiredSize, s, strLength) ==
            0) {
          return buffer;
        }
      }
    }

    return std::wstring();
  }

  HeapInformation _lastHeapInformation;
  std::map<CallbackKey, CallbackFunc> _callbacks;
  std::map<intptr_t, size_t> _largeMemBlocks;
};

std::unique_ptr<HermesRuntime> makeHermesRuntimeWithWER() {
  auto cm = std::make_shared<CrashManagerImpl>();
  return makeHermesRuntime(
      ::hermes::vm::RuntimeConfig::Builder().withCrashMgr(cm).build());
}

void hermesCrashHandler(HermesRuntime &runtime, int fd) {
  ::hermes::vm::Runtime &vmRuntime = getVMRuntime(runtime);

  // Run all callbacks registered to the crash manager
  auto &crashManager = vmRuntime.getCrashManager();
  if (auto *crashManagerImpl =
          dynamic_cast<CrashManagerImpl *>(&crashManager)) {
    crashManagerImpl->crashHandler(fd);
  }

  // Also serialize the current callstack
  auto callstack = vmRuntime.getCallStackNoAlloc();
  llvh::raw_fd_ostream jsonStream(fd, false);
  ::hermes::JSONEmitter json(jsonStream);
  json.openDict();
  json.emitKeyValue("callstack", callstack);
  json.closeDict();
  json.endJSONL();
}

class RuntimeWrapper {
 public:
  explicit RuntimeWrapper(bool withWER = false)
      : hermesRuntime_(
            withWER ? makeHermesRuntimeWithWER() : makeHermesRuntime()) {}

  hermes_status getNonAbiSafeRuntime(void **nonAbiSafeRuntime) {
    CHECK_ARG(nonAbiSafeRuntime);
    *nonAbiSafeRuntime = hermesRuntime_.get();
    return hermes_ok;
  }

  hermes_status dumpCrashData(int32_t fd) {
    hermesCrashHandler(*hermesRuntime_, fd);
    return hermes_ok;
  }

  hermes_status addToProfiler() {
    hermesRuntime_->registerForProfiling();
    return hermes_ok;
  }

  hermes_status removeFromProfiler() {
    hermesRuntime_->unregisterForProfiling();
    return hermes_ok;
  }

 private:
  std::unique_ptr<HermesRuntime> hermesRuntime_;
};

} // namespace facebook::hermes

hermes_status hermes_create_runtime(hermes_runtime *runtime) {
  CHECK_ARG(runtime);
  *runtime =
      reinterpret_cast<hermes_runtime>(new facebook::hermes::RuntimeWrapper());
  return hermes_ok;
}

hermes_status hermes_create_runtime_with_wer(hermes_runtime *runtime) {
  CHECK_ARG(runtime);
  *runtime = reinterpret_cast<hermes_runtime>(
      new facebook::hermes::RuntimeWrapper(/*withWER*/ true));
  return hermes_ok;
}

hermes_status hermes_delete_runtime(hermes_runtime runtime) {
  return hermes_ok;
}

hermes_status hermes_get_non_abi_safe_runtime(
    hermes_runtime runtime,
    void **non_abi_safe_runtime) {
  return CHECKED_RUNTIME(runtime)->getNonAbiSafeRuntime(non_abi_safe_runtime);
}

hermes_status hermes_dump_crash_data(hermes_runtime runtime, int32_t fd) {
  return CHECKED_RUNTIME(runtime)->dumpCrashData(fd);
}

hermes_status hermes_sampling_profiler_enable() {
  facebook::hermes::HermesRuntime::enableSamplingProfiler();
  return hermes_ok;
}

hermes_status hermes_sampling_profiler_disable() {
  facebook::hermes::HermesRuntime::disableSamplingProfiler();
  return hermes_ok;
}

hermes_status hermes_sampling_profiler_add(hermes_runtime runtime) {
  return CHECKED_RUNTIME(runtime)->addToProfiler();
}

hermes_status hermes_sampling_profiler_remove(hermes_runtime runtime) {
  return CHECKED_RUNTIME(runtime)->removeFromProfiler();
}

hermes_status hermes_sampling_profiler_dump_to_file(const char *filename) {
  facebook::hermes::HermesRuntime::dumpSampledTraceToFile(filename);
  return hermes_ok;
}
