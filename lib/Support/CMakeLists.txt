# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set(link_libs
    dtoa
    hermesPlatform
    hermesPlatformUnicode)

if (ANDROID)
  list(APPEND link_libs log)
endif()

add_hermes_library(hermesSupport
        Allocator.cpp
        Base64.cpp
        Base64vlq.cpp
        BigIntSupport.cpp
        CheckedMalloc.cpp
        Conversions.cpp
        ErrorHandling.cpp
        InternalIdentifierMaker.cpp
        JSONEmitter.cpp
        OSCompatCommon.cpp
        OSCompatEmscripten.cpp
        OSCompatPosix.cpp
        OSCompatWindows.cpp
        PageAccessTrackerPosix.cpp
        PerfSection.cpp
        SemaphorePosix.cpp
        SerialExecutor.cpp
        SHA1.cpp
        SNPrintfBuf.cpp
        SourceErrorManager.cpp
        SimpleDiagHandler.cpp
        StringTable.cpp
        UTF8.cpp
        UTF16Stream.cpp
        LEB128.cpp
        LINK_LIBS ${link_libs}
)
