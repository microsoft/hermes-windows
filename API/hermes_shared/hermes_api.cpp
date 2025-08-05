/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT license.
 */

#include "hermes_api.h"
#include "MurmurHash.h"
#include "ScriptStore.h"
#include "hermes/BCGen/HBC/BytecodeProviderFromSrc.h"
#include "hermes/VM/Callable.h"
#include "hermes/VM/Runtime.h"
#include "hermes/hermes.h"
#include "hermes/inspector/RuntimeAdapter.h"
#include "hermes/inspector/chrome/Registration.h"
#include "hermes_node_api.h"
#include "llvh/Support/raw_os_ostream.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <werapi.h>

#define CHECKED_RUNTIME(runtime) \
  (runtime == nullptr)           \
      ? napi_generic_failure     \
      : reinterpret_cast<facebook::hermes::RuntimeWrapper *>(runtime)

#define CHECKED_CONFIG(config) \
  (config == nullptr)          \
      ? napi_generic_failure   \
      : reinterpret_cast<facebook::hermes::ConfigWrapper *>(config)

#define CHECK_ARG(arg)           \
  if (arg == nullptr) {          \
    return napi_generic_failure; \
  }

#define CHECKED_ENV_RUNTIME(env)          \
  (env == nullptr) ? napi_generic_failure \
                   : facebook::hermes::RuntimeWrapper::from(env)

#define CHECK_STATUS(func)       \
  do {                           \
    napi_status status__ = func; \
    if (status__ != napi_ok) {   \
      return status__;           \
    }                            \
  } while (0)

// Return error status with message.
#define ERROR_STATUS(status, ...)         \
  ::hermes::node_api::setLastNativeError( \
      env_, (status), (__FILE__), (uint32_t)(__LINE__), __VA_ARGS__)

// Return napi_generic_failure with message.
#define GENERIC_FAILURE(...) ERROR_STATUS(napi_generic_failure, __VA_ARGS__)

namespace facebook::hermes {

union HermesBuildVersionInfo {
  struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint16_t revision;
  };
  uint64_t version;
};

constexpr HermesBuildVersionInfo HermesBuildVersion = {HERMES_FILE_VERSION_BIN};

::hermes::vm::Runtime *getVMRuntime(HermesRuntime &runtime) noexcept {
  ::facebook::hermes::IHermes *hermes =
      facebook::jsi::castInterface<::facebook::hermes::IHermes>(&runtime);
  return static_cast<::hermes::vm::Runtime *>(hermes->getVMRuntimeUnsafe());
}

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
          static_cast<uint32_t>(length - pieceCount * WER_MAX_MEM_BLOCK_SIZE));
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

void hermesCrashHandler(HermesRuntime &runtime, int fd) {
  ::hermes::vm::Runtime *vmRuntime = getVMRuntime(runtime);

  // Run all callbacks registered to the crash manager
  auto &crashManager = vmRuntime->getCrashManager();
  if (auto *crashManagerImpl =
          dynamic_cast<CrashManagerImpl *>(&crashManager)) {
    crashManagerImpl->crashHandler(fd);
  }

  // Also serialize the current callstack
  auto callstack = vmRuntime->getCallStackNoAlloc();
  llvh::raw_fd_ostream jsonStream(fd, false);
  ::hermes::JSONEmitter json(jsonStream);
  json.openDict();
  json.emitKeyValue("callstack", callstack);
  json.closeDict();
  json.endJSONL();
}

class Task : public ::hermes::node_api::Task {
 public:
  static void run(void *task) {
    reinterpret_cast<Task *>(task)->invoke();
  }

  static void deleteTask(void *task, void * /*deleterData*/) {
    delete reinterpret_cast<Task *>(task);
  }
};

template <typename TLambda>
class LambdaTask : public Task {
 public:
  LambdaTask(TLambda &&lambda) : lambda_(std::move(lambda)) {}

  void invoke() noexcept override {
    lambda_();
  }

 private:
  TLambda lambda_;
};

class TaskRunner : public ::hermes::node_api::TaskRunner {
 public:
  TaskRunner(
      void *data,
      jsr_task_runner_post_task_cb postTaskCallback,
      jsr_data_delete_cb deleteCallback,
      void *deleterData)
      : data_(data),
        postTaskCallback_(postTaskCallback),
        deleteCallback_(deleteCallback),
        deleterData_(deleterData) {}

  ~TaskRunner() {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(data_, deleterData_);
    }
  }

  void post(std::unique_ptr<::hermes::node_api::Task> task) noexcept override {
    postTaskCallback_(
        data_, task.release(), &Task::run, &Task::deleteTask, nullptr);
  }

 private:
  void *data_;
  jsr_task_runner_post_task_cb postTaskCallback_;
  jsr_data_delete_cb deleteCallback_;
  void *deleterData_;
};

class ScriptBuffer : public facebook::jsi::Buffer {
 public:
  ScriptBuffer(
      const uint8_t *data,
      size_t size,
      jsr_data_delete_cb deleteCallback,
      void *deleterData)
      : data_(data),
        size_(size),
        deleteCallback_(deleteCallback),
        deleterData_(deleterData) {}

  ~ScriptBuffer() {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(const_cast<uint8_t *>(data_), deleterData_);
    }
  }

  const uint8_t *data() const override {
    return data_;
  }

  size_t size() const override {
    return size_;
  }

  static void deleteBuffer(void * /*data*/, void *scriptBuffer) {
    delete reinterpret_cast<ScriptBuffer *>(scriptBuffer);
  }

 private:
  const uint8_t *data_{};
  size_t size_{};
  jsr_data_delete_cb deleteCallback_{};
  void *deleterData_{};
};

class ScriptCache : public facebook::jsi::PreparedScriptStore {
 public:
  ScriptCache(
      void *data,
      jsr_script_cache_load_cb loadCallback,
      jsr_script_cache_store_cb storeCallback,
      jsr_data_delete_cb deleteCallback,
      void *deleterData)
      : data_(data),
        loadCallback_(loadCallback),
        storeCallback_(storeCallback),
        deleteCallback_(deleteCallback),
        deleterData_(deleterData) {}

  ~ScriptCache() {
    if (deleteCallback_ != nullptr) {
      deleteCallback_(data_, deleterData_);
    }
  }

  std::shared_ptr<const facebook::jsi::Buffer> tryGetPreparedScript(
      const facebook::jsi::ScriptSignature &scriptSignature,
      const facebook::jsi::JSRuntimeSignature &runtimeMetadata,
      const char *prepareTag) noexcept override {
    const uint8_t *buffer{};
    size_t bufferSize{};
    jsr_data_delete_cb bufferDeleteCallback{};
    void *bufferDeleterData{};
    loadCallback_(
        data_,
        scriptSignature.url.c_str(),
        scriptSignature.version,
        runtimeMetadata.runtimeName.c_str(),
        runtimeMetadata.version,
        prepareTag,
        &buffer,
        &bufferSize,
        &bufferDeleteCallback,
        &bufferDeleterData);
    return std::make_shared<ScriptBuffer>(
        buffer, bufferSize, bufferDeleteCallback, bufferDeleterData);
  }

  void persistPreparedScript(
      std::shared_ptr<const facebook::jsi::Buffer> preparedScript,
      const facebook::jsi::ScriptSignature &scriptSignature,
      const facebook::jsi::JSRuntimeSignature &runtimeMetadata,
      const char *prepareTag) noexcept override {
    storeCallback_(
        data_,
        scriptSignature.url.c_str(),
        scriptSignature.version,
        runtimeMetadata.runtimeName.c_str(),
        runtimeMetadata.version,
        prepareTag,
        preparedScript->data(),
        preparedScript->size(),
        [](void * /*data*/, void *deleterData) {
          delete reinterpret_cast<
              std::shared_ptr<const facebook::jsi::Buffer> *>(deleterData);
        },
        new std::shared_ptr<const facebook::jsi::Buffer>(preparedScript));
  }

 private:
  void *data_{};
  jsr_script_cache_load_cb loadCallback_{};
  jsr_script_cache_store_cb storeCallback_{};
  jsr_data_delete_cb deleteCallback_{};
  void *deleterData_{};
};

class ConfigWrapper {
 public:
  napi_status enableDefaultCrashHandler(bool value) {
    enableDefaultCrashHandler_ = value;
    return napi_status::napi_ok;
  }

  napi_status enableInspector(bool value) {
    enableInspector_ = value;
    return napi_status::napi_ok;
  }

  napi_status setInspectorRuntimeName(std::string name) {
    inspectorRuntimeName_ = std::move(name);
    return napi_status::napi_ok;
  }

  napi_status setInspectorPort(uint16_t port) {
    inspectorPort_ = port;
    return napi_status::napi_ok;
  }

  napi_status setInspectorBreakOnStart(bool value) {
    inspectorBreakOnStart_ = value;
    return napi_status::napi_ok;
  }

  napi_status setExplicitMicrotasks(bool value) {
    explicitMicrotasks_ = value;
    return napi_status::napi_ok;
  }

  napi_status setUnhandledErrorCallback(
      std::function<void(napi_env, napi_value)> unhandledErrorCallback) {
    unhandledErrorCallback_ = unhandledErrorCallback;
    return napi_status::napi_ok;
  }

  napi_status setTaskRunner(std::unique_ptr<TaskRunner> taskRunner) {
    taskRunner_ = std::move(taskRunner);
    return napi_status::napi_ok;
  }

  napi_status setScriptCache(std::unique_ptr<ScriptCache> scriptCache) {
    scriptCache_ = std::move(scriptCache);
    return napi_status::napi_ok;
  }

  bool enableDefaultCrashHandler() {
    return enableDefaultCrashHandler_;
  }

  bool enableInspector() const {
    return enableInspector_;
  }

  const std::string &inspectorRuntimeName() const {
    return inspectorRuntimeName_;
  }

  uint16_t inspectorPort() {
    return inspectorPort_;
  }

  bool inspectorBreakOnStart() {
    return inspectorBreakOnStart_;
  }

  const std::shared_ptr<TaskRunner> &taskRunner() const {
    return taskRunner_;
  }

  const std::shared_ptr<ScriptCache> &scriptCache() const {
    return scriptCache_;
  }

  const std::function<void(napi_env, napi_value)> &unhandledErrorCallback()
      const {
    return unhandledErrorCallback_;
  }

  ::hermes::vm::RuntimeConfig getRuntimeConfig() const {
    ::hermes::vm::RuntimeConfig::Builder config;
    if (enableDefaultCrashHandler_) {
      auto crashManager = std::make_shared<CrashManagerImpl>();
      config.withCrashMgr(crashManager);
    }
    config.withMicrotaskQueue(explicitMicrotasks_);
    return config.build();
  }

 private:
  bool enableDefaultCrashHandler_{};
  bool enableInspector_{};
  std::string inspectorRuntimeName_;
  uint16_t inspectorPort_{};
  bool inspectorBreakOnStart_{};
  bool explicitMicrotasks_{};
  std::function<void(napi_env env, napi_value value)> unhandledErrorCallback_{};
  std::shared_ptr<TaskRunner> taskRunner_;
  std::shared_ptr<ScriptCache> scriptCache_;
};

class HermesRuntime;

class HermesExecutorRuntimeAdapter final
    : public facebook::hermes::inspector::RuntimeAdapter {
 public:
  HermesExecutorRuntimeAdapter(
      std::shared_ptr<facebook::hermes::HermesRuntime> hermesRuntime,
      std::shared_ptr<TaskRunner> taskRunner);

  virtual ~HermesExecutorRuntimeAdapter() = default;
  HermesRuntime &getRuntime() override;
  void tickleJs() override;

 private:
  std::shared_ptr<facebook::hermes::HermesRuntime> hermesJsiRuntime_;
  std::shared_ptr<TaskRunner> taskRunner_;
};

// An implementation of PreparedJavaScript that wraps a BytecodeProvider.
class NodeApiScriptModel final {
 public:
  explicit NodeApiScriptModel(
      std::unique_ptr<::hermes::hbc::BCProvider> bcProvider,
      ::hermes::vm::RuntimeModuleFlags runtimeFlags,
      std::string sourceURL,
      bool isBytecode)
      : bcProvider_(std::move(bcProvider)),
        runtimeFlags_(runtimeFlags),
        sourceURL_(std::move(sourceURL)),
        isBytecode_(isBytecode) {}

  std::shared_ptr<::hermes::hbc::BCProvider> bytecodeProvider() const {
    return bcProvider_;
  }

  ::hermes::vm::RuntimeModuleFlags runtimeFlags() const {
    return runtimeFlags_;
  }

  const std::string &sourceURL() const {
    return sourceURL_;
  }

  bool isBytecode() const {
    return isBytecode_;
  }

 private:
  std::shared_ptr<::hermes::hbc::BCProvider> bcProvider_;
  ::hermes::vm::RuntimeModuleFlags runtimeFlags_;
  std::string sourceURL_;
  bool isBytecode_{false};
};

// Wraps script data as hermes::Buffer
class ScriptDataBuffer final : public ::hermes::Buffer {
 public:
  ScriptDataBuffer(
      const uint8_t *scriptData,
      size_t scriptLength,
      jsr_data_delete_cb scriptDeleteCallback,
      void *deleterData) noexcept
      : Buffer(scriptData, scriptLength),
        scriptDeleteCallback_(scriptDeleteCallback),
        deleterData_(deleterData) {}

  ~ScriptDataBuffer() noexcept override {
    if (scriptDeleteCallback_ != nullptr) {
      scriptDeleteCallback_(const_cast<uint8_t *>(data()), deleterData_);
    }
  }

  ScriptDataBuffer(const ScriptDataBuffer &) = delete;
  ScriptDataBuffer &operator=(const ScriptDataBuffer &) = delete;

 private:
  jsr_data_delete_cb scriptDeleteCallback_{};
  void *deleterData_{};
};

class JsiBuffer final : public ::hermes::Buffer {
 public:
  JsiBuffer(std::shared_ptr<const facebook::jsi::Buffer> buffer) noexcept
      : Buffer(buffer->data(), buffer->size()), buffer_(std::move(buffer)) {}

 private:
  std::shared_ptr<const facebook::jsi::Buffer> buffer_;
};

class JsiSmallVectorBuffer final : public facebook::jsi::Buffer {
 public:
  JsiSmallVectorBuffer(llvh::SmallVector<char, 0> data) noexcept
      : data_(std::move(data)) {}

  size_t size() const override {
    return data_.size();
  }

  const uint8_t *data() const override {
    return reinterpret_cast<const uint8_t *>(data_.data());
  }

 private:
  llvh::SmallVector<char, 0> data_;
};

class RuntimeWrapper {
 public:
  explicit RuntimeWrapper(const ConfigWrapper &config)
      : hermesJsiRuntime_(makeHermesRuntime(config.getRuntimeConfig())),
        hermesVMRuntime_(*getVMRuntime(*hermesJsiRuntime_)),
        isInspectable_(config.enableInspector()),
        scriptCache_(config.scriptCache()) {
    if (isInspectable_) {
      compileFlags_.debug = true;
    }
    ::hermes::vm::RuntimeConfig runtimeConfig = config.getRuntimeConfig();
    switch (runtimeConfig.getCompilationMode()) {
      case ::hermes::vm::SmartCompilation:
        compileFlags_.lazy = true;
        // (Leaves thresholds at default values)
        break;
      case ::hermes::vm::ForceEagerCompilation:
        compileFlags_.lazy = false;
        break;
      case ::hermes::vm::ForceLazyCompilation:
        compileFlags_.lazy = true;
        compileFlags_.preemptiveFileCompilationThreshold = 0;
        compileFlags_.preemptiveFunctionCompilationThreshold = 0;
        break;
    }

    compileFlags_.enableGenerator = runtimeConfig.getEnableGenerator();
    compileFlags_.emitAsyncBreakCheck =
        runtimeConfig.getAsyncBreakCheckInEval();

    ::hermes::vm::CallResult<napi_env> envRes =
        ::hermes::node_api::getOrCreateNodeApiEnvironment(
            hermesVMRuntime_,
            compileFlags_,
            config.taskRunner(),
            config.unhandledErrorCallback(),
            NAPI_VERSION_EXPERIMENTAL);

    if (envRes.getStatus() == ::hermes::vm::ExecutionStatus::EXCEPTION) {
      throw std::runtime_error("Failed to create Node API environment");
    }
    env_ = envRes.getValue();
    ::hermes::node_api::setNodeApiEnvironmentData(
        env_, kRuntimeWrapperTag, this);

    if (config.enableInspector()) {
      auto adapter = std::make_unique<HermesExecutorRuntimeAdapter>(
          hermesJsiRuntime_, config.taskRunner());
      std::string inspectorRuntimeName = config.inspectorRuntimeName();
      if (inspectorRuntimeName.empty()) {
        inspectorRuntimeName = "Hermes";
      }
      facebook::hermes::inspector::chrome::enableDebugging(
          std::move(adapter), inspectorRuntimeName);
    }
  }

  static facebook::hermes::RuntimeWrapper *from(napi_env env) {
    if (env == nullptr) {
      return nullptr;
    }

    void *data{};
    ::hermes::node_api::getNodeApiEnvironmentData(
        env, kRuntimeWrapperTag, &data);
    return reinterpret_cast<facebook::hermes::RuntimeWrapper *>(data);
  }

  napi_status dumpCrashData(int32_t fd) {
    hermesCrashHandler(*hermesJsiRuntime_, fd);
    return napi_ok;
  }

  napi_status addToProfiler() {
    hermesJsiRuntime_->registerForProfiling();
    return napi_ok;
  }

  napi_status removeFromProfiler() {
    hermesJsiRuntime_->unregisterForProfiling();
    return napi_ok;
  }

  napi_status getNodeApi(napi_env *env) {
    *env = env_;
    return napi_ok;
  }

  napi_status getDescription(const char **result) noexcept {
    CHECK_ARG(result);
    *result = "Hermes";
    return napi_ok;
  }

  napi_status isInspectable(bool *result) noexcept {
    CHECK_ARG(result);
    *result = isInspectable_;
    return napi_ok;
  }

  napi_status drainMicrotasks(int32_t maxCountHint, bool *result) noexcept {
    CHECK_ARG(result);
    if (hermesVMRuntime_.hasMicrotaskQueue()) {
      CHECK_STATUS(
          ::hermes::node_api::checkJSErrorStatus(
              env_, hermesVMRuntime_.drainJobs()));
    }

    hermesVMRuntime_.clearKeptObjects();
    *result = true;
    return napi_ok;
  }

  //---------------------------------------------------------------------------
  // Script running
  //---------------------------------------------------------------------------

  // Run script from a string value.
  // The sourceURL is used only for error reporting.
  napi_status runScript(
      napi_value source,
      const char *sourceURL,
      napi_value *result) noexcept {
    CHECK_ARG(source);
    size_t sourceSize{};
    CHECK_STATUS(
        napi_get_value_string_utf8(env_, source, nullptr, 0, &sourceSize));
    std::unique_ptr<char[]> buffer =
        std::unique_ptr<char[]>(new char[sourceSize + 1]);
    CHECK_STATUS(napi_get_value_string_utf8(
        env_, source, buffer.get(), sourceSize + 1, nullptr));

    jsr_prepared_script preparedScript{};
    CHECK_STATUS(createPreparedScript(
        reinterpret_cast<uint8_t *>(buffer.release()),
        sourceSize,
        [](void *data, void * /*deleterData*/) {
          std::unique_ptr<char[]> buf(reinterpret_cast<char *>(data));
        },
        nullptr,
        sourceURL,
        &preparedScript));
    // To delete prepared script after execution.
    std::unique_ptr<NodeApiScriptModel> scriptModel{
        reinterpret_cast<NodeApiScriptModel *>(preparedScript)};

    return runPreparedScript(preparedScript, result);
  }

  napi_status createPreparedScript(
      const uint8_t *scriptData,
      size_t scriptLength,
      jsr_data_delete_cb scriptDeleteCallback,
      void *deleterData,
      const char *sourceURL,
      jsr_prepared_script *result) noexcept {
    std::unique_ptr<ScriptDataBuffer> buffer =
        std::make_unique<ScriptDataBuffer>(
            scriptData, scriptLength, scriptDeleteCallback, deleterData);

    std::pair<std::unique_ptr<::hermes::hbc::BCProvider>, std::string> bcErr{};
    ::hermes::vm::RuntimeModuleFlags runtimeFlags{};
    runtimeFlags.persistent = true;

    bool isBytecode = isHermesBytecode(buffer->data(), buffer->size());
    // Save the first few bytes of the buffer so that we can later append them
    // to any error message.
    uint8_t bufPrefix[16];
    const size_t bufSize = buffer->size();
    std::memcpy(
        bufPrefix, buffer->data(), std::min(sizeof(bufPrefix), bufSize));

    // Construct the BC provider either from buffer or source.
    if (isBytecode) {
      bcErr = ::hermes::hbc::BCProviderFromBuffer::createBCProviderFromBuffer(
          std::move(buffer));
    } else {
#if defined(HERMESVM_LEAN)
      bcErr.second = "prepareJavaScript source compilation not supported";
#else

      facebook::jsi::ScriptSignature scriptSignature;
      facebook::jsi::JSRuntimeSignature runtimeSignature;
      const char *prepareTag = "perf";

      if (scriptCache_) {
        uint64_t hash{};
        bool isAscii = murmurhash(buffer->data(), buffer->size(), /*ref*/ hash);
        facebook::jsi::JSRuntimeVersion_t runtimeVersion =
            HermesBuildVersion.version;
        scriptSignature = {std::string(sourceURL ? sourceURL : ""), hash};
        runtimeSignature = {"Hermes", runtimeVersion};
      }

      std::shared_ptr<const facebook::jsi::Buffer> cache;
      if (scriptCache_) {
        cache = scriptCache_->tryGetPreparedScript(
            scriptSignature, runtimeSignature, prepareTag);
        bcErr = ::hermes::hbc::BCProviderFromBuffer::createBCProviderFromBuffer(
            std::make_unique<JsiBuffer>(move(cache)));
      }

      ::hermes::hbc::BCProviderFromSrc *bytecodeProviderFromSrc{};
      if (!bcErr.first) {
        std::
            pair<std::unique_ptr<::hermes::hbc::BCProviderFromSrc>, std::string>
                bcFromSrcErr =
                    ::hermes::hbc::BCProviderFromSrc::createBCProviderFromSrc(
                        std::move(buffer),
                        std::string(sourceURL ? sourceURL : ""),
                        nullptr,
                        compileFlags_);
        bytecodeProviderFromSrc = bcFromSrcErr.first.get();
        bcErr = std::move(bcFromSrcErr);
      }

      if (scriptCache_ && bytecodeProviderFromSrc) {
        ::hermes::hbc::BytecodeModule *bcModule =
            bytecodeProviderFromSrc->getBytecodeModule();

        // Serialize/deserialize can't handle lazy compilation as of now. Do a
        // check to make sure there is no lazy BytecodeFunction in module_.
        for (uint32_t i = 0; i < bcModule->getNumFunctions(); i++) {
          if (bytecodeProviderFromSrc->isFunctionLazy(i)) {
            goto CannotSerialize;
          }
        }

        // Serialize the bytecode. Call BytecodeSerializer to do the heavy
        // lifting. Write to a SmallVector first, so we can know the total bytes
        // and write it first and make life easier for Deserializer. This is
        // going to be slower than writing to Serializer directly but it's OK to
        // slow down serialization if it speeds up Deserializer.
        ::hermes::BytecodeGenerationOptions bytecodeGenOpts =
            ::hermes::BytecodeGenerationOptions::defaults();
        llvh::SmallVector<char, 0> bytecodeVector;
        llvh::raw_svector_ostream outStream(bytecodeVector);
        ::hermes::hbc::BytecodeSerializer bcSerializer{
            outStream, bytecodeGenOpts};
        bcSerializer.serialize(
            *bcModule, bytecodeProviderFromSrc->getSourceHash());

        scriptCache_->persistPreparedScript(
            std::shared_ptr<const facebook::jsi::Buffer>(
                new JsiSmallVectorBuffer(std::move(bytecodeVector))),
            scriptSignature,
            runtimeSignature,
            prepareTag);
      }
#endif
    }
    if (!bcErr.first) {
      std::string errorMessage;
      llvh::raw_string_ostream stream(errorMessage);
      stream << " Buffer size: " << bufSize << ", starts with: ";
      for (size_t i = 0; i < sizeof(bufPrefix) && i < bufSize; ++i) {
        stream << llvh::format_hex_no_prefix(bufPrefix[i], 2);
      }
      return GENERIC_FAILURE(
          "Compiling JS failed: ", bcErr.second, stream.str());
    }

#if !defined(HERMESVM_LEAN)
  CannotSerialize:
#endif
    *result = reinterpret_cast<jsr_prepared_script>(new NodeApiScriptModel(
        std::move(bcErr.first),
        runtimeFlags,
        sourceURL ? sourceURL : "",
        isBytecode));
    return ::hermes::node_api::clearLastNativeError(env_);
  }

  napi_status deletePreparedScript(
      jsr_prepared_script preparedScript) noexcept {
    CHECK_ARG(preparedScript);
    delete reinterpret_cast<NodeApiScriptModel *>(preparedScript);
    return ::hermes::node_api::clearLastNativeError(env_);
  }

  napi_status runPreparedScript(
      jsr_prepared_script preparedScript,
      napi_value *result) noexcept {
    CHECK_ARG(preparedScript);
    const NodeApiScriptModel *hermesPrep =
        reinterpret_cast<NodeApiScriptModel *>(preparedScript);
    return ::hermes::node_api::runBytecode(
        env_,
        hermesPrep->bytecodeProvider(),
        hermesPrep->runtimeFlags(),
        hermesPrep->sourceURL(),
        result);
  }

  // Internal function to check if buffer contains Hermes VM bytecode.
  static bool isHermesBytecode(const uint8_t *data, size_t len) noexcept {
    return ::hermes::hbc::BCProviderFromBuffer::isBytecodeStream(
        llvh::ArrayRef<uint8_t>(data, len));
  }

  napi_status initializeNativeModule(
      napi_addon_register_func register_module,
      int32_t api_version,
      napi_value *exports) noexcept {
    return ::hermes::node_api::initializeNodeApiModule(
        hermesVMRuntime_, register_module, api_version, exports);
  }

 private:
  std::shared_ptr<HermesRuntime> hermesJsiRuntime_;
  ::hermes::vm::Runtime &hermesVMRuntime_;
  napi_env env_;

  // Can we run a debugger?
  bool isInspectable_{};

  // Optional prepared script store.
  std::shared_ptr<facebook::jsi::PreparedScriptStore> scriptCache_{};

  // Flags used by byte code compiler.
  ::hermes::hbc::CompileFlags compileFlags_{};

  static constexpr napi_type_tag kRuntimeWrapperTag{
      0xfa327a491b4b4d20,
      0x94407c81c2d4e8f2};
};

HermesExecutorRuntimeAdapter::HermesExecutorRuntimeAdapter(
    std::shared_ptr<facebook::hermes::HermesRuntime> hermesRuntime,
    std::shared_ptr<TaskRunner> taskRunner)
    : hermesJsiRuntime_(std::move(hermesRuntime)),
      taskRunner_(std::move(taskRunner)) {}

HermesRuntime &HermesExecutorRuntimeAdapter::getRuntime() {
  return *hermesJsiRuntime_;
}

void HermesExecutorRuntimeAdapter::tickleJs() {
  // The queue will ensure that hermesJsiRuntime_ is still valid when this
  // gets invoked.
  taskRunner_->post(
      std::unique_ptr<Task>(new LambdaTask([&runtime = *hermesJsiRuntime_]() {
        auto func =
            runtime.global().getPropertyAsFunction(runtime, "__tickleJs");
        func.call(runtime);
      })));
}

} // namespace facebook::hermes

JSR_API
jsr_create_runtime(jsr_config config, jsr_runtime *runtime) {
  CHECK_ARG(config);
  CHECK_ARG(runtime);
  *runtime = reinterpret_cast<jsr_runtime>(new facebook::hermes::RuntimeWrapper(
      *reinterpret_cast<facebook::hermes::ConfigWrapper *>(config)));
  return napi_ok;
}

JSR_API jsr_delete_runtime(jsr_runtime runtime) {
  CHECK_ARG(runtime);
  delete reinterpret_cast<facebook::hermes::RuntimeWrapper *>(runtime);
  return napi_ok;
}

JSR_API jsr_runtime_get_node_api_env(jsr_runtime runtime, napi_env *env) {
  return CHECKED_RUNTIME(runtime)->getNodeApi(env);
}

JSR_API hermes_dump_crash_data(jsr_runtime runtime, int32_t fd) {
  return CHECKED_RUNTIME(runtime)->dumpCrashData(fd);
}

static facebook::hermes::IHermesRootAPI *getHermesRootAPI() {
  // The makeHermesRootAPI returns a singleton.
  return facebook::jsi::castInterface<facebook::hermes::IHermesRootAPI>(
      facebook::hermes::makeHermesRootAPI());
}

JSR_API hermes_sampling_profiler_enable() {
  getHermesRootAPI()->enableSamplingProfiler();
  return napi_ok;
}

JSR_API hermes_sampling_profiler_disable() {
  getHermesRootAPI()->disableSamplingProfiler();
  return napi_ok;
}

JSR_API hermes_sampling_profiler_add(jsr_runtime runtime) {
  return CHECKED_RUNTIME(runtime)->addToProfiler();
}

JSR_API hermes_sampling_profiler_remove(jsr_runtime runtime) {
  return CHECKED_RUNTIME(runtime)->removeFromProfiler();
}

JSR_API hermes_sampling_profiler_dump_to_file(const char *filename) {
  getHermesRootAPI()->dumpSampledTraceToFile(filename);
  return napi_ok;
}

JSR_API jsr_create_config(jsr_config *config) {
  CHECK_ARG(config);
  *config = reinterpret_cast<jsr_config>(new facebook::hermes::ConfigWrapper());
  return napi_ok;
}

JSR_API jsr_delete_config(jsr_config config) {
  CHECK_ARG(config);
  delete reinterpret_cast<facebook::hermes::ConfigWrapper *>(config);
  return napi_ok;
}

JSR_API hermes_config_enable_default_crash_handler(
    jsr_config config,
    bool value) {
  return CHECKED_CONFIG(config)->enableDefaultCrashHandler(value);
}

JSR_API jsr_config_enable_inspector(jsr_config config, bool value) {
  return CHECKED_CONFIG(config)->enableInspector(value);
}

JSR_API jsr_config_set_inspector_runtime_name(
    jsr_config config,
    const char *name) {
  return CHECKED_CONFIG(config)->setInspectorRuntimeName(name);
}

JSR_API jsr_config_set_inspector_port(jsr_config config, uint16_t port) {
  return CHECKED_CONFIG(config)->setInspectorPort(port);
}

JSR_API jsr_config_set_inspector_break_on_start(jsr_config config, bool value) {
  return CHECKED_CONFIG(config)->setInspectorBreakOnStart(value);
}

JSR_API jsr_config_enable_gc_api(jsr_config /*config*/, bool /*value*/) {
  // We do nothing for now.
  return napi_ok;
}

JSR_API jsr_config_set_explicit_microtasks(jsr_config config, bool value) {
  return CHECKED_CONFIG(config)->setExplicitMicrotasks(value);
}

JSR_API jsr_config_set_task_runner(
    jsr_config config,
    void *task_runner_data,
    jsr_task_runner_post_task_cb task_runner_post_task_cb,
    jsr_data_delete_cb task_runner_data_delete_cb,
    void *deleter_data) {
  return CHECKED_CONFIG(config)->setTaskRunner(
      std::make_unique<facebook::hermes::TaskRunner>(
          task_runner_data,
          task_runner_post_task_cb,
          task_runner_data_delete_cb,
          deleter_data));
}

JSR_API jsr_config_on_unhandled_error(
    jsr_config config,
    void *cb_data,
    jsr_unhandled_error_cb unhandled_error_cb) {
  return CHECKED_CONFIG(config)->setUnhandledErrorCallback(
      [cb_data, unhandled_error_cb](napi_env env, napi_value error) {
        unhandled_error_cb(cb_data, env, error);
      });
}

JSR_API jsr_config_set_script_cache(
    jsr_config config,
    void *script_cache_data,
    jsr_script_cache_load_cb script_cache_load_cb,
    jsr_script_cache_store_cb script_cache_store_cb,
    jsr_data_delete_cb script_cache_data_delete_cb,
    void *deleter_data) {
  return CHECKED_CONFIG(config)->setScriptCache(
      std::make_unique<facebook::hermes::ScriptCache>(
          script_cache_data,
          script_cache_load_cb,
          script_cache_store_cb,
          script_cache_data_delete_cb,
          deleter_data));
}

//=============================================================================
// Node-API extensions to host JS engine and to implement JSI
//=============================================================================

JSR_API jsr_collect_garbage(napi_env env) {
  return hermes::node_api::collectGarbage(env);
}

JSR_API
jsr_has_unhandled_promise_rejection(napi_env env, bool *result) {
  return hermes::node_api::hasUnhandledPromiseRejection(env, result);
}

JSR_API jsr_get_and_clear_last_unhandled_promise_rejection(
    napi_env env,
    napi_value *result) {
  return hermes::node_api::getAndClearLastUnhandledPromiseRejection(
      env, result);
}

JSR_API jsr_get_description(napi_env env, const char **result) {
  return CHECKED_ENV_RUNTIME(env)->getDescription(result);
}

JSR_API jsr_queue_microtask(napi_env env, napi_value callback) {
  return hermes::node_api::queueMicrotask(env, callback);
}

JSR_API
jsr_drain_microtasks(napi_env env, int32_t max_count_hint, bool *result) {
  return CHECKED_ENV_RUNTIME(env)->drainMicrotasks(max_count_hint, result);
}

JSR_API jsr_is_inspectable(napi_env env, bool *result) {
  return CHECKED_ENV_RUNTIME(env)->isInspectable(result);
}

JSR_API jsr_open_napi_env_scope(napi_env /*env*/, jsr_napi_env_scope *scope) {
  if (scope != nullptr) {
    *scope = nullptr;
  }
  return napi_ok;
}

JSR_API jsr_close_napi_env_scope(
    napi_env /*env*/,
    jsr_napi_env_scope /*scope*/) {
  return napi_ok;
}

//-----------------------------------------------------------------------------
// Script preparing and running.
//
// Script is usually converted to byte code, or in other words - prepared - for
// execution. Then, we can run the prepared script.
//-----------------------------------------------------------------------------

JSR_API jsr_run_script(
    napi_env env,
    napi_value source,
    const char *source_url,
    napi_value *result) {
  return CHECKED_ENV_RUNTIME(env)->runScript(source, source_url, result);
}

JSR_API jsr_create_prepared_script(
    napi_env env,
    const uint8_t *script_data,
    size_t script_length,
    jsr_data_delete_cb script_delete_cb,
    void *deleter_data,
    const char *source_url,
    jsr_prepared_script *result) {
  return CHECKED_ENV_RUNTIME(env)->createPreparedScript(
      script_data,
      script_length,
      script_delete_cb,
      deleter_data,
      source_url,
      result);
}

JSR_API
jsr_delete_prepared_script(napi_env env, jsr_prepared_script prepared_script) {
  return CHECKED_ENV_RUNTIME(env)->deletePreparedScript(prepared_script);
}

JSR_API jsr_prepared_script_run(
    napi_env env,
    jsr_prepared_script prepared_script,
    napi_value *result) {
  return CHECKED_ENV_RUNTIME(env)->runPreparedScript(prepared_script, result);
}

JSR_API jsr_initialize_native_module(
    napi_env env,
    napi_addon_register_func register_module,
    int32_t api_version,
    napi_value *exports) {
  return CHECKED_ENV_RUNTIME(env)->initializeNativeModule(
      register_module, api_version, exports);
}
