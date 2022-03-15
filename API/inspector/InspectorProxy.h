/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "jsinspector/InspectorInterfaces.h"

namespace facebook {
namespace react {

struct InspectorPage2 {
  int id;
  std::string title;
  std::string vm;
};

struct IInspectorPages {
  virtual InspectorPage2 getPage(int n) = 0;
  virtual int size() = 0;
  virtual ~IInspectorPages() = 0;
};

class JSINSPECTOR_EXPORT IRemoteConnection2 : public IDestructible {
 public:
  virtual ~IRemoteConnection2() = 0;
  virtual void onMessage(std::string message) = 0;
  virtual void onDisconnect() = 0;
};

extern __declspec(
    dllexport) std::unique_ptr<IInspectorPages> __cdecl getInspectorPages();

extern __declspec(dllexport)
    std::unique_ptr<ILocalConnection> __cdecl
  connectInspectorPage(int pageId, std::unique_ptr<IRemoteConnection2> remote);

} // namespace react
} // namespace facebook
