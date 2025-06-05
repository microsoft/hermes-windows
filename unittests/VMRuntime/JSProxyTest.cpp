/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes/VM/JSProxy.h"
#include "TestHelpers.h"
#include "hermes/VM/Callable.h"
#include "hermes/VM/JSObject.h"
#include "hermes/VM/Runtime.h"

using namespace hermes::vm;

namespace {

using JSProxyTest = RuntimeTestFixture;

TEST_F(JSProxyTest, CreateProxyTest) {
  // Create a proxy.
  ASSERT_NO_THROW(JSProxy::create(runtime));

  // Create a handler object.
  auto handler = runtime.makeHandle<JSObject>(JSObject::create(runtime));
  // Create a proxy with the handler.
  ASSERT_NO_THROW(JSProxy::create(runtime, handler));
}

/*
TEST_F(JSProxyTest, OwnPropertyKeysTest) {
    // Create selfHandle.
    auto selfHandle = runtime.makeHandle<JSObject>(JSObject::create(runtime));

    //call ownPropertyKeys on selfHandle and runtime.
    auto result = JSProxy::ownPropertyKeys(selfHandle, runtime, OwnKeysFlags{});
    ASSERT_RETURNED(result.getStatus());
}
*/

} // namespace