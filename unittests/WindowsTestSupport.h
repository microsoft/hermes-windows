// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef HERMES_UNITTESTS_WINDOWSTESTSUPPORT_H
#define HERMES_UNITTESTS_WINDOWSTESTSUPPORT_H

#ifdef _WIN32
#include <windows.h>
#include <cstdlib>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#endif

namespace hermes {
namespace testing {

inline void disableCrashDialogs() {
#ifdef _WIN32
  // Match the LLVM/gtest test drivers, without installing a stack-trace handler.
  // In particular, child test executables do not inherit their parent's CRT flags.
  ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _MSC_VER
  ::_set_error_mode(_OUT_TO_STDERR);
  ::_set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
}

} // namespace testing
} // namespace hermes

#endif
