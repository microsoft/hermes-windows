/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes_win.h"
#include "ScriptStore.h"
#include "hermes/VM/Runtime.h"
#include "hermes/inspector/RuntimeAdapter.h"
#include "hermes/inspector/chrome/Registration.h"
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

napi_status hermes_create_napi_env(
    ::hermes::vm::Runtime &runtime,
    bool isInspectable,
    std::shared_ptr<facebook::jsi::PreparedScriptStore> preparedScript,
    const ::hermes::vm::RuntimeConfig &runtimeConfig,
    napi_env *env);

napi_status jsr_env_unref(napi_env env);

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

class Task {
 public:
  virtual void invoke() noexcept = 0;

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

class TaskRunner {
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

  void post(std::unique_ptr<Task> task) {
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

  ::hermes::vm::RuntimeConfig getRuntimeConfig() const {
    ::hermes::vm::RuntimeConfig::Builder config;
    if (enableDefaultCrashHandler_) {
      auto crashManager = std::make_shared<CrashManagerImpl>();
      config.withCrashMgr(crashManager);
    }
    return config.build();
  }

 private:
  bool enableDefaultCrashHandler_{};
  bool enableInspector_{};
  std::string inspectorRuntimeName_;
  uint16_t inspectorPort_{};
  bool inspectorBreakOnStart_{};
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
  std::shared_ptr<facebook::hermes::HermesRuntime> hermesRuntime_;
  std::shared_ptr<TaskRunner> taskRunner_;
};

class RuntimeWrapper {
 public:
  explicit RuntimeWrapper(const ConfigWrapper &config)
      : hermesRuntime_(makeHermesRuntime(config.getRuntimeConfig())),
        vmRuntime_(getVMRuntime(*hermesRuntime_)) {
    hermes_create_napi_env(
        vmRuntime_, config.enableInspector(), config.scriptCache(), {}, &env_);

    if (config.enableInspector()) {
      auto adapter = std::make_unique<HermesExecutorRuntimeAdapter>(
          hermesRuntime_, config.taskRunner());
      std::string inspectorRuntimeName = config.inspectorRuntimeName();
      if (inspectorRuntimeName.empty()) {
        inspectorRuntimeName = "Hermes";
      }
      facebook::hermes::inspector::chrome::enableDebugging(
          std::move(adapter), inspectorRuntimeName);
    }
  }

  ~RuntimeWrapper() {
    jsr_env_unref(env_);
  }

  napi_status getNonAbiSafeRuntime(void **nonAbiSafeRuntime) {
    CHECK_ARG(nonAbiSafeRuntime);
    *nonAbiSafeRuntime = hermesRuntime_.get();
    return napi_ok;
  }

  napi_status dumpCrashData(int32_t fd) {
    hermesCrashHandler(*hermesRuntime_, fd);
    return napi_ok;
  }

  napi_status addToProfiler() {
    hermesRuntime_->registerForProfiling();
    return napi_ok;
  }

  napi_status removeFromProfiler() {
    hermesRuntime_->unregisterForProfiling();
    return napi_ok;
  }

  napi_status getNodeApi(napi_env *env) {
    *env = env_;
    return napi_ok;
  }

 private:
  std::shared_ptr<HermesRuntime> hermesRuntime_;
  ::hermes::vm::Runtime &vmRuntime_;
  napi_env env_;
};

HermesExecutorRuntimeAdapter::HermesExecutorRuntimeAdapter(
    std::shared_ptr<facebook::hermes::HermesRuntime> hermesRuntime,
    std::shared_ptr<TaskRunner> taskRunner)
    : hermesRuntime_(std::move(hermesRuntime)),
      taskRunner_(std::move(taskRunner)) {}

HermesRuntime &HermesExecutorRuntimeAdapter::getRuntime() {
  return *hermesRuntime_;
}

void HermesExecutorRuntimeAdapter::tickleJs() {
  // The queue will ensure that hermesRuntime_ is still valid when this gets
  // invoked.
  taskRunner_->post(
      std::unique_ptr<Task>(new LambdaTask([&runtime = *hermesRuntime_]() {
        auto func =
            runtime.global().getPropertyAsFunction(runtime, "__tickleJs");
        func.call(runtime);
      })));
}

} // namespace facebook::hermes

JSR_API jsr_create_runtime(jsr_config config, jsr_runtime *runtime) {
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

JSR_API hermes_sampling_profiler_enable() {
  facebook::hermes::HermesRuntime::enableSamplingProfiler();
  return napi_ok;
}

JSR_API hermes_sampling_profiler_disable() {
  facebook::hermes::HermesRuntime::disableSamplingProfiler();
  return napi_ok;
}

JSR_API hermes_sampling_profiler_add(jsr_runtime runtime) {
  return CHECKED_RUNTIME(runtime)->addToProfiler();
}

JSR_API hermes_sampling_profiler_remove(jsr_runtime runtime) {
  return CHECKED_RUNTIME(runtime)->removeFromProfiler();
}

JSR_API hermes_sampling_profiler_dump_to_file(const char *filename) {
  facebook::hermes::HermesRuntime::dumpSampledTraceToFile(filename);
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
