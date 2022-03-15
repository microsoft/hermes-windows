/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "InspectorProxy.h"

#include <mutex>
#include <tuple>
#include <unordered_map>

namespace facebook {
namespace react {

IInspectorPages::~IInspectorPages() {}
IRemoteConnection2::~IRemoteConnection2() {}

struct ProxyRemoteConnection : public IRemoteConnection {
  ProxyRemoteConnection(std::unique_ptr<IRemoteConnection2> remote)
      : remote_(std::move(remote)) {}
  std::unique_ptr<IRemoteConnection2> remote_;

  virtual void onMessage(std::string message) override {
    remote_->onMessage(message);
  }

  virtual void onDisconnect() override{
    remote_->onDisconnect();
  }
};

extern __declspec(dllexport)
std::unique_ptr<ILocalConnection> connectInspectorPage(
  int pageId,
  std::unique_ptr<IRemoteConnection2> remote) {
  IInspector &inspector = getInspectorInstance();
  std::unique_ptr<ILocalConnection> local = inspector.connect(
      pageId, std::make_unique<ProxyRemoteConnection>(std::move(remote)));
  return local;
}

struct InspectorPagesImpl : public IInspectorPages {
  virtual InspectorPage2 getPage(int n) {
    InspectorPage page = pages_[n];
    return InspectorPage2{
        page.id,
        page.title,
        page.vm};
  }

  virtual int size() {
    return pages_.size();
  }

  std::vector<InspectorPage> pages_;
  InspectorPagesImpl(std::vector<InspectorPage> &&pages)
      : pages_(std::move(pages)) {}
  ~InspectorPagesImpl() {}
};

extern __declspec(
    dllexport) std::unique_ptr<IInspectorPages> __cdecl getInspectorPages() {
  IInspector &inspector = getInspectorInstance();
  return std::make_unique<InspectorPagesImpl>(inspector.getPages());
}

} // namespace react
} // namespace facebook
