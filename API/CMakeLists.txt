# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set(HERMES_ENABLE_EH ON)
set(HERMES_ENABLE_RTTI ON)

add_subdirectory(node-api)

if (WIN32 AND HERMES_ENABLE_DEBUGGER)
    add_subdirectory(inspector)
endif()

add_subdirectory(hermes)
