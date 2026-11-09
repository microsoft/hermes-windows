# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

add_subdirectory(hermes)
add_subdirectory(hermes_abi)
add_subdirectory(hermes_node_api)
add_subdirectory(hermes_node_api_jsi)
# Upstream syncs land the regenerated wasm2c sources of the sandbox in a
# separate pull request, because GitHub cannot diff them in place. Build the
# sandbox only when those sources are present (or when generating them).
set(sandboxGenerated
  "${CMAKE_CURRENT_SOURCE_DIR}/hermes_sandbox/external/hermes_sandbox_impl")
# Re-run CMake when the sources are added or removed, so that existing build
# trees follow.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/hermes_sandbox/external")
if(EMSCRIPTEN OR (EXISTS "${sandboxGenerated}_dbg_compiled_0.c" AND
                  EXISTS "${sandboxGenerated}_opt_compiled_0.c"))
  add_subdirectory(hermes_sandbox)
else()
  message(STATUS "Generated sandbox sources are missing; skipping the sandbox.")
endif()
add_subdirectory(hermes_shared)
if(HERMES_ENABLE_NAPI)
  add_subdirectory(napi)
endif()
