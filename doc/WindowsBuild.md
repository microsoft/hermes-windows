# Windows Build System Design

## Overview

This document outlines the design for refactoring the Hermes Windows build system to eliminate duplication and provide clear separation of concerns across three key components: CMake files, CMake presets, and build scripts.

## Current State Problems

The current build system has several issues:

1. **Fragmented Configuration**: Build settings are scattered across multiple files
   - `build.js` - Build orchestration and dynamic flag generation
   - `CMakePresets.json` - Preset definitions with embedded flags
   - `CMakeLists.txt` and `*.cmake` files - Core build logic

2. **Duplication**: Similar flags and settings are duplicated across files
   - Compiler flags appear in both presets and CMake files
   - Linker flags are repeated in multiple presets
   - Environment variable handling is inconsistent

3. **Unclear Ownership**: No clear guidance on where to modify settings
   - Developers unsure whether to change presets or CMake files
   - Flag modifications require changes in multiple places
   - Inconsistent patterns for adding new configurations

## Proposed Architecture

### Component Roles

#### 1. CMake Files (`cmake/HermesWindows.cmake`)
**Role**: Single source of truth for all compiler and linker flags
**Responsibilities**:
- Define all compiler-specific flags (Clang vs MSVC)
- Define all linker-specific flags (lld-link vs MSVC linker)
- Handle platform-specific optimizations (`/OPT:REF`, `/guard:cf`, etc.)
- Provide functions for conditional flag application
- Handle cross-compilation target settings

#### 2. CMake Presets (`CMakePresets.json`)
**Role**: Developer-friendly configuration interface
**Responsibilities**:
- Define high-level build configurations (Debug/Release, Clang/MSVC)
- Set CMake variables that influence flag selection
- Provide environment variable pass-through for dynamic configuration
- Enable direct usage from VS Code and Visual Studio

#### 3. Build Script (`build.js`)
**Role**: Advanced build orchestration and CI/CD integration
**Responsibilities**:
- Handle complex build scenarios (UWP, cross-compilation)
- Orchestrate multi-step builds (tools → target)
- Set environment variables for preset customization
- Provide CI/CD pipeline integration
- Handle advanced scenarios not covered by presets

### Information Flow

```
build.js → Environment Variables → CMakePresets.json → CMake Variables → cmake/HermesWindows.cmake → Final Flags
```

## Detailed Design

### 1. cmake/HermesWindows.cmake

Create a new file that centralizes all Windows-specific build logic:

```cmake
# cmake/HermesWindows.cmake - Single source of truth for Windows build flags

# High-level configuration variables (set by presets or build.js)
option(HERMES_WINDOWS_USE_CLANG "Use Clang compiler" ON)
option(HERMES_WINDOWS_ENABLE_SECURITY_FLAGS "Enable security hardening flags" ON)
option(HERMES_WINDOWS_ENABLE_OPTIMIZATION "Enable linker optimizations" ON)
option(HERMES_WINDOWS_UWP_BUILD "Build for UWP" OFF)
option(HERMES_WINDOWS_CROSS_COMPILE "Cross-compilation build" OFF)

# Platform detection and validation
function(hermes_windows_detect_platform)
    # Detect target platform and set internal variables
endfunction()

# Compiler flag configuration
function(hermes_windows_configure_compiler_flags)
    if(HERMES_WINDOWS_USE_CLANG)
        hermes_windows_configure_clang_flags()
    else()
        hermes_windows_configure_msvc_flags()
    endif()
endfunction()

# Linker flag configuration
function(hermes_windows_configure_linker_flags)
    if(HERMES_WINDOWS_USE_CLANG)
        hermes_windows_configure_lld_flags()
    else()
        hermes_windows_configure_msvc_linker_flags()
    endif()
endfunction()

# Security flags (CFG, stack protection, etc.)
function(hermes_windows_apply_security_flags)
    # Apply security flags based on compiler and configuration
endfunction()

# Cross-compilation support
function(hermes_windows_configure_cross_compilation target_arch)
    # Configure cross-compilation flags for target architecture
endfunction()

# Main configuration function
function(hermes_windows_configure_build)
    hermes_windows_detect_platform()
    hermes_windows_configure_compiler_flags()
    hermes_windows_configure_linker_flags()
    
    if(HERMES_WINDOWS_ENABLE_SECURITY_FLAGS)
        hermes_windows_apply_security_flags()
    endif()
    
    if(HERMES_WINDOWS_CROSS_COMPILE)
        hermes_windows_configure_cross_compilation(${TARGET_ARCHITECTURE})
    endif()
endfunction()
```

### 2. CMakePresets.json Refactoring

Simplify presets to focus on high-level configuration:

```json
{
  "configurePresets": [
    {
      "name": "base-common",
      "hidden": true,
      "cacheVariables": {
        "HERMES_WINDOWS_USE_CLANG": "$env{HERMES_WINDOWS_USE_CLANG}",
        "HERMES_WINDOWS_ENABLE_SECURITY_FLAGS": "$env{HERMES_WINDOWS_ENABLE_SECURITY_FLAGS}",
        "HERMES_WINDOWS_UWP_BUILD": "$env{HERMES_WINDOWS_UWP_BUILD}",
        "HERMES_WINDOWS_CROSS_COMPILE": "$env{HERMES_WINDOWS_CROSS_COMPILE}"
      }
    },
    {
      "name": "ninja-clang-debug",
      "inherits": "base-common",
      "displayName": "Ninja + Clang (Debug)",
      "environment": {
        "HERMES_WINDOWS_USE_CLANG": "ON",
        "HERMES_WINDOWS_ENABLE_SECURITY_FLAGS": "ON",
        "HERMES_WINDOWS_UWP_BUILD": "OFF",
        "HERMES_WINDOWS_CROSS_COMPILE": "OFF"
      },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "FastDebug"
      }
    }
  ]
}
```

### 3. build.js Integration

Modify build.js to work with the new architecture:

```javascript
function configureBuildEnvironment(buildParams) {
    const configEnv = {};
    
    // Set high-level configuration flags
    configEnv.HERMES_WINDOWS_USE_CLANG = args.msvc ? "OFF" : "ON";
    configEnv.HERMES_WINDOWS_UWP_BUILD = buildParams.isUwp ? "ON" : "OFF";
    configEnv.HERMES_WINDOWS_CROSS_COMPILE = isCrossPlatformBuild(buildParams) ? "ON" : "OFF";
    
    // Let CMake handle the complex flag logic
    return configEnv;
}
```

## Migration Strategy

### Phase 1: Create cmake/HermesWindows.cmake
1. Create the new CMake file with all Windows-specific functions
2. Migrate existing flag logic from CMakeLists.txt
3. Test with current preset configurations

### Phase 2: Simplify CMakePresets.json
1. Remove embedded compiler/linker flags from presets
2. Replace with high-level configuration variables
3. Add environment variable pass-through
4. Validate VS Code and Visual Studio integration

### Phase 3: Update build.js
1. Modify to set high-level environment variables
2. Remove flag duplication and hardcoded values
3. Test all build scenarios (native, cross-compile, UWP)

### Phase 4: Cleanup and Documentation
1. Remove obsolete flag definitions from CMakeLists.txt
2. Update documentation and examples
3. Add validation tests for all configurations

## Benefits

### 1. Single Source of Truth
- All compiler/linker flags in one place (`cmake/HermesWindows.cmake`)
- Eliminates duplication and inconsistencies
- Easier to maintain and update

### 2. Clear Separation of Concerns
- **CMake**: Technical implementation details
- **Presets**: Developer-friendly configurations
- **build.js**: Advanced orchestration and CI/CD

### 3. Improved Maintainability
- Flag changes require modification in only one place
- Clear ownership model for different types of changes
- Easier to add new configurations

### 4. Better Developer Experience
- Presets remain simple and focused
- build.js handles complex scenarios transparently
- Clear documentation on where to make changes

## Configuration Examples

### Adding a New Compiler Flag

**Before** (requires changes in 3 places):
```json
// CMakePresets.json
"CMAKE_CXX_FLAGS": "...existing flags... -new-flag"
```
```javascript
// build.js
configEnv.HERMES_ADDITIONAL_CXX_FLAGS = "...existing... -new-flag";
```
```cmake
# CMakeLists.txt
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -new-flag")
```

**After** (single change):
```cmake
# cmake/HermesWindows.cmake
function(hermes_windows_configure_clang_flags)
    list(APPEND HERMES_WINDOWS_CXX_FLAGS "-new-flag")
endfunction()
```

### Adding a New Build Configuration

**Before** (complex preset with embedded flags):
```json
{
  "name": "new-config",
  "cacheVariables": {
    "CMAKE_CXX_FLAGS": "-DWIN32 -D_WINDOWS ... -new-config-flag",
    "CMAKE_C_FLAGS": "-DWIN32 -D_WINDOWS ... -new-config-flag"
  }
}
```

**After** (simple high-level configuration):
```json
{
  "name": "new-config",
  "inherits": "base-common",
  "environment": {
    "HERMES_WINDOWS_NEW_FEATURE": "ON"
  }
}
```

## Testing Strategy

### Unit Tests
- Test individual CMake functions in isolation
- Validate flag generation for different configurations
- Test environment variable handling

### Integration Tests
- Test preset configurations with VS Code/Visual Studio
- Validate build.js scenarios (native, cross-compile, UWP)
- Test CI/CD pipeline integration

### Regression Tests
- Ensure all existing build scenarios continue to work
- Validate binary compatibility and performance
- Test security flag effectiveness

## Future Enhancements

### 1. Configuration Validation
- Add CMake functions to validate configuration combinations
- Provide clear error messages for invalid settings
- Prevent incompatible flag combinations

### 2. Performance Optimization
- Cache flag computation results
- Optimize preset loading time
- Minimize redundant CMake evaluations

### 3. Enhanced Documentation
- Auto-generate configuration documentation
- Provide interactive configuration examples
- Add troubleshooting guides

## Current State Analysis

Looking at the current `CMakePresets.json`, we can see the exact duplication issues:

### Current Flag Duplication in Presets

**Clang Preset**:
```json
"CMAKE_CXX_FLAGS": "-DWIN32 -D_WINDOWS -D_CRT_RAND_S -g -gcodeview -fstack-protector-all $env{HERMES_ADDITIONAL_CXX_FLAGS}",
"CMAKE_C_FLAGS": "-DWIN32 -D_WINDOWS -D_CRT_RAND_S -g -gcodeview -fstack-protector-all $env{HERMES_ADDITIONAL_C_FLAGS}",
"CMAKE_EXE_LINKER_FLAGS": "-Wl,/DEBUG:FULL -Wl,/guard:cf -Wl,/CETCOMPAT $env{HERMES_ADDITIONAL_EXE_LINKER_FLAGS}",
"CMAKE_SHARED_LINKER_FLAGS": "-Wl,/DEBUG:FULL -Wl,/guard:cf -Wl,/CETCOMPAT $env{HERMES_ADDITIONAL_SHARED_LINKER_FLAGS}"
```

**MSVC Preset**:
```json
"CMAKE_EXE_LINKER_FLAGS": "/DEBUG:FULL /guard:cf $env{HERMES_ADDITIONAL_EXE_LINKER_FLAGS}",
"CMAKE_SHARED_LINKER_FLAGS": "/DEBUG:FULL /guard:cf $env{HERMES_ADDITIONAL_SHARED_LINKER_FLAGS}"
```

### Problems with Current Approach

1. **Flag Duplication**: Basic flags like `-DWIN32`, `-D_WINDOWS`, `-g` are repeated
2. **Compiler-Specific Logic**: Different flag syntax for Clang vs MSVC embedded in presets
3. **Security Flag Hardcoding**: Security flags like `/guard:cf` are hardcoded in presets
4. **Environment Variable Proliferation**: Need separate env vars for each flag type

### Proposed Solution Impact

After refactoring, the presets would become:

```json
{
  "name": "base-ninja-clang",
  "inherits": "base-common",
  "environment": {
    "HERMES_WINDOWS_USE_CLANG": "ON",
    "HERMES_WINDOWS_ENABLE_SECURITY_FLAGS": "ON"
  },
  "cacheVariables": {
    "CMAKE_SYSTEM_NAME": "Windows",
    "CMAKE_SYSTEM_PROCESSOR": "AMD64"
  }
}
```

All the complex flag logic would move to `cmake/HermesWindows.cmake`, eliminating duplication and making the presets much cleaner and more maintainable.

## Conclusion

This refactoring will create a more maintainable, scalable, and developer-friendly build system. By centralizing technical details in CMake while keeping high-level configuration accessible through presets and build scripts, we achieve the best of both worlds: simplicity for common use cases and power for advanced scenarios.

The phased migration approach ensures minimal disruption while providing immediate benefits as each phase is completed.
