/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../hermes_api.h"
#include "jsinspector/InspectorInterfaces.h"

#include <mutex>
#include <tuple>
#include <unordered_map>

namespace facebook::react {

// pure destructors in C++ are odd. You would think they don't want an
// implementation, but in fact the linker requires one. Define them to be
// empty so that people don't count on them for any particular behaviour.
IDestructible::~IDestructible() {}
ILocalConnection::~ILocalConnection() {}
IRemoteConnection::~IRemoteConnection() {}
IInspector::~IInspector() {}

namespace {

class RemoteConnectionWrapper : public IRemoteConnection {
 public:
  static std::unique_ptr<IRemoteConnection> create(
      hermes_remote_connection remoteConnection,
      hermes_remote_connection_send_message_cb onSendMessageCallback,
      hermes_remote_connection_disconnect_cb onDisconnectCallback,
      jsr_data_delete_cb onDeleteCallback,
      void *deleterData) {
    RemoteConnectionWrapper *connection = new RemoteConnectionWrapper();
    connection->remoteConnection_ = remoteConnection;
    connection->onSendMessageCallback_ = onSendMessageCallback;
    connection->onDisconnectCallback_ = onDisconnectCallback;
    connection->onDeleteCallback_ = onDeleteCallback;
    connection->deleterData_ = deleterData;
    return std::unique_ptr<IRemoteConnection>(connection);
  }

  ~RemoteConnectionWrapper() override {
    onDeleteCallback_(remoteConnection_, deleterData_);
  }

  void onMessage(std::string message) override {
    onSendMessageCallback_(remoteConnection_, message.c_str());
  }

  void onDisconnect() override {
    onDisconnectCallback_(remoteConnection_);
  }

 private:
  hermes_remote_connection remoteConnection_;
  hermes_remote_connection_send_message_cb onSendMessageCallback_;
  hermes_remote_connection_disconnect_cb onDisconnectCallback_;
  jsr_data_delete_cb onDeleteCallback_;
  void *deleterData_;
};

class InspectorProxy : public IInspector {
 public:
  int addPage(
      const std::string &title,
      const std::string &vm,
      ConnectFunc connectFunc) {
    std::unique_ptr<ConnectFunc> connectFuncPtr =
        std::make_unique<ConnectFunc>(std::move(connectFunc));
    int32_t pageId =
        addPageCallback_(title.c_str(), vm.c_str(), connectFuncPtr.get());
    std::lock_guard<std::mutex> lock(mutex_);
    connectFuncs_[pageId] = std::move(connectFuncPtr);
    return pageId;
  }

  void removePage(int pageId) {
    removePageCallback_(pageId);
    std::lock_guard<std::mutex> lock(mutex_);
    connectFuncs_.erase(pageId);
  }

  std::vector<InspectorPage> getPages() const {
    throw new std::exception("Not implemented in Hermes VM");
  }

  std::unique_ptr<ILocalConnection> connect(
      int /*pageId*/,
      std::unique_ptr<IRemoteConnection> /*remote*/) {
    throw new std::exception("Not implemented in Hermes VM");
  }

  void setInspector(
      hermes_inspector_add_page_cb addPageCallback,
      hermes_inspector_remove_page_cb removePageCallback) {
    addPageCallback_ = addPageCallback;
    removePageCallback_ = removePageCallback;
  }

 private:
  mutable std::mutex mutex_;
  std::unordered_map<int32_t, std::unique_ptr<ConnectFunc>> connectFuncs_;
  hermes_inspector_add_page_cb addPageCallback_{};
  hermes_inspector_remove_page_cb removePageCallback_{};
};

} // namespace

InspectorProxy &getInspectorProxyInstance() {
  static InspectorProxy instance;
  return instance;
}

IInspector &getInspectorInstance() {
  return getInspectorProxyInstance();
}

} // namespace facebook::react

JSR_API hermes_set_inspector(
    hermes_inspector_add_page_cb add_page_cb,
    hermes_inspector_remove_page_cb remove_page_cb) {
  facebook::react::getInspectorProxyInstance().setInspector(
      add_page_cb, remove_page_cb);
  return napi_ok;
}

JSR_API hermes_create_local_connection(
    void *connect_func,
    hermes_remote_connection remote_connection,
    hermes_remote_connection_send_message_cb on_send_message_cb,
    hermes_remote_connection_disconnect_cb on_disconnect_cb,
    jsr_data_delete_cb on_delete_cb,
    void *deleter_data,
    hermes_local_connection *local_connection) {
  auto connectFunc =
      reinterpret_cast<facebook::react::IInspector::ConnectFunc *>(
          connect_func);
  std::unique_ptr<facebook::react::IRemoteConnection> remoteConnection =
      facebook::react::RemoteConnectionWrapper::create(
          remote_connection,
          on_send_message_cb,
          on_disconnect_cb,
          on_delete_cb,
          deleter_data);
  std::unique_ptr<facebook::react::ILocalConnection> localConnection =
      (*connectFunc)(std::move(remoteConnection));
  *local_connection =
      reinterpret_cast<hermes_local_connection>(localConnection.release());
  return napi_ok;
}

JSR_API
hermes_delete_local_connection(hermes_local_connection local_connection) {
  delete reinterpret_cast<facebook::react::ILocalConnection *>(
      local_connection);
  return napi_ok;
}

JSR_API hermes_local_connection_send_message(
    hermes_local_connection local_connection,
    const char *message) {
  reinterpret_cast<facebook::react::ILocalConnection *>(local_connection)
      ->sendMessage(message);
  return napi_ok;
}

JSR_API
hermes_local_connection_disconnect(hermes_local_connection local_connection) {
  reinterpret_cast<facebook::react::ILocalConnection *>(local_connection)
      ->disconnect();
  return napi_ok;
}
