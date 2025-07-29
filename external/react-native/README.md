# React Native External Dependency

## Overview
This directory contains React Native components required for Hermes inspector functionality. The files are sourced from the React Native repository to eliminate FetchContent warnings and improve build performance.

## Source Information
- **Repository**: https://github.com/facebook/react-native
- **Commit**: a0ed073650cbdc3e1e08c11d18acf73e0952393a
- **Date**: May 2, 2024
- **Version**: React Native ~0.74
- **Download URL**: https://github.com/facebook/react-native/archive/a0ed073650cbdc3e1e08c11d18acf73e0952393a.zip
- **SHA256**: 8111f994cbef91300ed57f9bbdd8a0d20fe27bab1ad9a6bfcd637c67a4a29af5

## Components Included

### 1. ReactCommon/hermes
- **Purpose**: Hermes-specific React Native integration components
- **Key Files**: 
  - `inspector-modern/chrome/CDPHandler.h`
  - `inspector-modern/chrome/HermesRuntimeAgentDelegate.h`
  - `inspector-modern/chrome/HermesRuntimeAgentDelegate.cpp`
- **Dependencies**: Requires jsinspector-modern and runtimeexecutor

### 2. ReactCommon/jsinspector-modern
- **Purpose**: Modern JavaScript inspector framework
- **Key Files**:
  - `ReactCdp.h` - React Chrome DevTools Protocol definitions
  - `FallbackRuntimeAgentDelegate.h` - Fallback implementation when debugger disabled
- **Usage**: Included via `#include <jsinspector-modern/ReactCdp.h>`

### 3. ReactCommon/runtimeexecutor
- **Purpose**: Runtime execution utilities
- **Key Files**:
  - `ReactCommon/RuntimeExecutor.h` - Runtime executor interface
- **Usage**: Included via `#include <ReactCommon/RuntimeExecutor.h>`

## Applied Overrides
- **HermesRuntimeAgentDelegate.cpp**: Custom override from `API/hermes_shared/inspector/react-native/overrides/`

## Directory Structure
```
external/react-native/
├── README.md                           # This documentation
├── download_react_native.ps1           # Download and setup script
└── packages/react-native/
    └── ReactCommon/
        ├── hermes/                     # Hermes-specific components
        │   └── inspector-modern/
        │       └── chrome/
        │           ├── CDPHandler.h
        │           ├── HermesRuntimeAgentDelegate.h
        │           └── HermesRuntimeAgentDelegate.cpp
        ├── jsinspector-modern/         # JS inspector framework
        │   ├── ReactCdp.h
        │   └── FallbackRuntimeAgentDelegate.h
        └── runtimeexecutor/            # Runtime execution utilities
            └── ReactCommon/
                └── RuntimeExecutor.h
```
## Build Integration
The React Native components are integrated via `API/hermes_shared/inspector/react-native/CMakeLists.txt`:

```cmake
# Verify external react-native directory exists
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external/react-native")
    message(FATAL_ERROR "External react-native directory not found. Please run download_react_native.ps1")
endif()

# Set React Native source directory
set(REACT_NATIVE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external/react-native/packages/react-native")

# Include React Native ReactCommon directories
target_include_directories(hermesinspector PRIVATE
    "${REACT_NATIVE_SOURCE}"
    "${REACT_NATIVE_SOURCE}/ReactCommon"
)
```

## CMake Integration
This external dependency replaces the FetchContent mechanism:

**Before (FetchContent)**:
```cmake
FetchContent_Declare(
    react-native
    GIT_REPOSITORY https://github.com/facebook/react-native.git
    GIT_TAG a0ed073650cbdc3e1e08c11d18acf73e0952393a
)
FetchContent_MakeAvailable(react-native)
```

**After (External)**:
- No more FetchContent warnings
- Faster CMake configuration
- Repository-safe (no embedded .git directories)
- Consistent with other external dependencies

## Setup Instructions
This directory was set up by running the automated script:

```powershell
cd external/react-native
.\download_react_native.ps1
```

The script performs:
1. **Downloads React Native commit a0ed073650cbdc3e1e08c11d18acf73e0952393a**
2. **Extracts required ReactCommon components**: hermes, jsinspector-modern, runtimeexecutor
3. **Applies overrides**: Copies `HermesRuntimeAgentDelegate.cpp` override
4. **Cleans up**: Removes Git repositories and unnecessary files
5. **Validates**: Ensures all required files are present

## Conversion Benefits
- ✅ **Eliminates FetchContent warnings** - No more deprecation messages
- ✅ **Faster configuration** - No network downloads during CMake configure
- ✅ **Better CI reliability** - No network dependencies during build
- ✅ **Offline builds** - Can build without internet connectivity
- ✅ **Essential components only** - hermes, jsinspector-modern, runtimeexecutor
- ✅ **Repository-safe** - No Git metadata, safe to add to your repository

## Usage
The React Native components are used by the Hermes inspector functionality. The paths are automatically configured via the `REACT_NATIVE_SOURCE` variable in CMake.

## Compilation Requirements
- HERMES_ENABLE_DEBUGGER must be defined for full functionality
- Requires C++17 or later
- Compatible with MSVC 1928+ (Visual Studio 2019 16.8+)

## Update Process
To update React Native components to a new version:

1. **Run the download script with new commit hash**:
   ```powershell
   .\download_react_native.ps1 -CommitHash "new_commit_hash"
   ```

2. **Test compilation** to ensure compatibility
3. **Update this README** with new version information

## Maintenance
- Monitor React Native releases for security updates
- Test compatibility with new versions before updating
- Keep documentation synchronized with actual implementation

## Notes
- Only essential ReactCommon components are included (hermes, jsinspector-modern, runtimeexecutor)
- Git repositories are cleaned during setup to maintain repository safety
- Files are copied locally to avoid network dependencies during build
- Override files take precedence over extracted React Native files
   Remove-Item packages -Recurse -Force
   # Extract new archive
   Expand-Archive react-native.zip
   # Copy only needed folder
   New-Item -ItemType Directory -Path "packages\react-native\ReactCommon" -Force
   Copy-Item "react-native-[COMMIT_HASH]\packages\react-native\ReactCommon\hermes" -Destination "packages\react-native\ReactCommon\" -Recurse -Force
   # Apply override
   Copy-Item "..\..\API\hermes_shared\inspector\react-native\overrides\HermesRuntimeAgentDelegate.cpp" -Destination "packages\react-native\ReactCommon\hermes\inspector-modern\chrome\" -Force
   # Clean up
   Remove-Item react-native-[COMMIT_HASH] -Recurse -Force
   Remove-Item react-native.zip
   ```

3. **Update this README** with new commit hash and SHA256
4. **Test the build** to ensure compatibility

## Technical Notes
- Only the Hermes-related components are included from React Native
- Original archive contained the full React Native codebase (~15MB compressed)
- Current structure contains only essential files for Hermes inspector functionality
- Override file is automatically applied during CMake configuration
- No Git metadata - safe for inclusion in your repository

## Maintenance
- **Source repository**: https://github.com/facebook/react-native
- **Commit reference**: https://github.com/facebook/react-native/commit/a0ed073650cbdc3e1e08c11d18acf73e0952393a
- **Documentation**: https://reactnative.dev/docs/hermes

Successfully converted from FetchContent to external dependency on July 28, 2025.
