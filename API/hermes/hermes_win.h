/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef HERMES_HERMES_WIN_H
#define HERMES_HERMES_WIN_H

#include "hermes.h"

namespace facebook::hermes {

HERMES_EXPORT std::unique_ptr<HermesRuntime> __cdecl makeHermesRuntimeWithWER();
HERMES_EXPORT void __cdecl hermesCrashHandler(HermesRuntime &runtime, int fd);

} // namespace facebook::hermes

#endif // HERMES_HERMES_WIN_H
