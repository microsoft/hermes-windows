# Debugging Windows Memory Issues

This document provides a comprehensive guide for debugging heap corruption, memory leaks, and
use-after-free issues on Windows using various debugging tools and techniques.

## Table of Contents

1. [Application Verifier](#application-verifier)
2. [Page Heap (GFlags)](#page-heap-gflags)
3. [Visual Studio Debugging](#visual-studio-debugging)
4. [Child Process Debugging](#child-process-debugging)
5. [CRT Debug Heap](#crt-debug-heap)
6. [Common Memory Issues](#common-memory-issues)
7. [Cleanup Commands](#cleanup-commands)

## Application Verifier

Application Verifier is a runtime verification tool that monitors application behavior and can
detect heap corruption, handle leaks, and other issues.

### Setup

```powershell
# Launch Application Verifier UI
appverif.exe

# Enable heap verification for your executable
appverif.exe -enable Heaps -for YourApp.exe

# Enable additional checks
appverif.exe -enable Heaps Handles Locks -for YourApp.exe
```

### Application Verifier UI Configuration

1. **Launch**: Start Menu → "Application Verifier" or run `appverif.exe`
2. **Add Application**: 
   - Click "Add Application" 
   - Browse to your executable (e.g., `NodeApiTests.exe`, `node-lite.exe`)
3. **Enable Tests**:
   - Check "Heaps" for heap corruption detection
   - Check "Handles" for handle leak detection
   - Check "Locks" for lock verification
4. **Heap Properties** (Right-click "Heaps" → Properties):
   - ✅ "Collect stack traces for allocations"
   - Set "Stack trace depth" to 20-30
   - ✅ "Enable page heap"
   - ✅ "Enable heap free checking"

### What Application Verifier Detects

- Double-delete/double-free
- Heap buffer overruns
- Use-after-free
- Handle leaks
- Critical section issues
- Stack overflow

### Interpreting Results

When Application Verifier detects an issue:
- It will break into the debugger at the exact point of corruption
- Check the call stack for the problematic code
- Look at the "Verifier" output window for detailed information
- The first chance exception shows WHERE the corruption is detected
- You may need to examine earlier allocations to find the root cause

## Page Heap (GFlags)

Page Heap provides immediate detection of heap corruption by placing guard pages around allocations.

### Setup

```powershell
# Enable full page heap (most thorough, high memory usage)
gflags.exe /p /enable YourApp.exe /full

# Enable normal page heap (balanced approach)
gflags.exe /p /enable YourApp.exe

# Enable with stack traces (requires symbol path)
gflags.exe /p /enable YourApp.exe /full /backwards

# Check current settings
gflags.exe /p
```

### GFlags Options

- `/full`: Full page heap - every allocation gets guard pages
- `/backwards`: Collect stack traces for allocations/deallocations
- `/random`: Random percentage of allocations get guard pages
- `/size`: Only allocations of specific size get guard pages

### Architecture Considerations

```powershell
# For 64-bit applications, use 64-bit tools
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\gflags.exe"

# For 32-bit applications, use 32-bit tools  
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\gflags.exe"
```

### What Page Heap Detects

- Buffer overruns (immediate crash)
- Use-after-free (immediate crash)
- Double-delete (immediate crash)
- Provides exact call stack at the point of corruption

## Visual Studio Debugging

### Basic Setup

1. **Debug Configuration**: Ensure you're building with debug symbols (`/Zi`, `/DEBUG:FULL`)
2. **Disable Optimizations**: Use Debug configuration or `/Od` flag
3. **Enable CRT Debug Heap**: Add `/D_CRTDBG_MAP_ALLOC` to compiler flags

### Advanced Debugging Features

```cpp
// Add to main() or DllMain() for comprehensive CRT debugging
#ifdef _DEBUG
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
  
  // Break on specific allocation number if known
  // _CrtSetBreakAlloc(allocation_number);
  
  // Enable heap corruption detection
  _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_CHECK_CRT_DF);
#endif
```

### Debugging Multi-Process Applications

For applications that spawn child processes (like NodeApiTests.exe → node-lite.exe):

#### Visual Studio Extensions Required:
- **Microsoft Child Process Debugging Power Tool 2022**:
  https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool2022

#### Setup in Visual Studio:
1. Install the Child Process Debugging Power Tool
2. **Debug → Other Debug Targets → Child Process Debugging Settings**
3. Check "Enable child process debugging"
4. Add your child executable (e.g., `node-lite.exe`)

## Child Process Debugging

### VS Code Setup

#### Required Extension:
- **Child Process Debugger**:
  https://marketplace.visualstudio.com/items?itemName=albertziegenhagel.childdebugger

#### Configuration (launch.json):

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug NodeApiTests with Child Processes",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/vs2022-msvc-x64/unittests/NodeApi/Debug/NodeApiTests.exe",
      "args": ["--gtest_output=xml:${workspaceFolder}/test-results.xml"],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "console": "externalTerminal",
      "enableChildProcessDebugging": true,
      "childProcessDebuggerExtension": {
        "enabled": true,
        "breakOnProcessCreation": true,
        "attachToChildProcesses": [
          {
            "name": "node-lite.exe",
            "enabled": true
          }
        ]
      }
    }
  ]
}
```

### Manual Attachment

If automatic attachment doesn't work:

1. **VS Code**: Use "Attach to Process" and look for `node-lite.exe`
2. **Visual Studio**: Debug → Attach to Process → select `node-lite.exe`
3. **Command Line**: 
   ```powershell
   # Start with debugger attached
   windbg.exe -o -g YourApp.exe
   ```

## CRT Debug Heap

### Compile-Time Setup

```cmake
# In CMakeLists.txt
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(your_target PRIVATE 
    _CRTDBG_MAP_ALLOC
    _DEBUG
    DEBUG_HEAP
  )
endif()
```

### Runtime Setup

```cpp
// Custom allocation tracking
#ifdef TRACK_HEAP_OPERATIONS
#include <crtdbg.h>
#include <unordered_map>
#include <string>
#include <mutex>

struct AllocationInfo {
    std::string allocStack;
    std::string deallocStack;
    bool isDeleted = false;
    size_t size = 0;
};

std::unordered_map<void*, AllocationInfo> g_allocations;
std::mutex g_alloc_mutex;

// Get current call stack
std::string GetCallStack() {
    void* stack[64];
    WORD frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    
    std::stringstream ss;
    for (WORD i = 0; i < frames; i++) {
        ss << "0x" << std::hex << (uintptr_t)stack[i] << "\n";
    }
    return ss.str();
}

// Custom allocation hook for tracking double-deletes
int AllocHook(int allocType, void* userData, size_t size, int blockType, 
              long requestNumber, const unsigned char* filename, int lineNumber) {
    
    std::lock_guard<std::mutex> lock(g_alloc_mutex);
    
    if (allocType == _HOOK_ALLOC) {
        g_allocations[userData] = {GetCallStack(), "", false, size};
        
        char msg[256];
        sprintf_s(msg, "ALLOC: %p size=%zu at %s:%d\n", userData, size, 
                 filename ? (const char*)filename : "unknown", lineNumber);
        OutputDebugStringA(msg);
    }
    else if (allocType == _HOOK_FREE && userData) {
        auto it = g_allocations.find(userData);
        if (it != g_allocations.end()) {
            if (it->second.isDeleted) {
                // DOUBLE DELETE DETECTED!
                char msg[1024];
                sprintf_s(msg, "DOUBLE DELETE DETECTED!\nPtr: %p\n"
                         "Original allocation:\n%s\n"
                         "First delete:\n%s\n"
                         "Second delete:\n%s\n",
                         userData,
                         it->second.allocStack.c_str(),
                         it->second.deallocStack.c_str(),
                         GetCallStack().c_str());
                OutputDebugStringA(msg);
                DebugBreak();
            } else {
                it->second.deallocStack = GetCallStack();
                it->second.isDeleted = true;
            }
        }
    }
    
    return TRUE;
}

// Initialize heap tracking
void InitializeHeapTracking() {
    _CrtSetAllocHook(AllocHook);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
}
#endif
```

## Common Memory Issues

### Use-After-Free

**Symptoms**: Crash when accessing freed memory, corrupted data
**Detection**: Page Heap, Application Verifier
**Example**:
```cpp
// Problem code
std::unique_ptr<Object> ptr = std::make_unique<Object>();
Object* rawPtr = ptr.get();
ptr.reset(); // Object destroyed
rawPtr->method(); // Use-after-free!
```

### Double-Delete

**Symptoms**: Heap corruption, crashes in destructor
**Detection**: Application Verifier, CRT Debug Heap
**Example**:
```cpp
// Problem code  
std::unique_ptr<Object> ptr1 = std::make_unique<Object>();
std::unique_ptr<Object> ptr2 = std::move(ptr1);
delete ptr1.get(); // Double delete when ptr2 destructs!
```

### Heap Buffer Overrun

**Symptoms**: Corrupted adjacent memory, crashes
**Detection**: Page Heap (immediate), Application Verifier
**Example**:
```cpp
// Problem code
char* buffer = new char[10];
strcpy(buffer, "This string is too long!"); // Overrun!
```

### Memory Leaks

**Symptoms**: Increasing memory usage over time
**Detection**: CRT Debug Heap, Application Verifier
**Example**:
```cpp
// Problem code
void function() {
    Object* obj = new Object();
    // Missing: delete obj;
}
```

## Cleanup Commands

### Remove All Debug Settings

```powershell
# Application Verifier cleanup
appverif.exe -disable * -for NodeApiTests.exe
appverif.exe -disable * -for node-lite.exe

# Page Heap cleanup  
gflags.exe /p /disable NodeApiTests.exe
gflags.exe /p /disable node-lite.exe

# Check what's still enabled
appverif.exe
gflags.exe /p

# Registry cleanup (if needed)
# Remove entries from:
#  HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\
```

### Verify Cleanup

```powershell
# List all applications with verifier settings
appverif.exe

# List all applications with page heap settings  
gflags.exe /p

# Both should show no entries for your applications
```

## Stack Size Configuration

For applications experiencing stack overflow with debugging tools:

```cmake
# Increase stack size in CMakeLists.txt
if (WIN32)
  if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:20971520") # 20MB
  elseif(CLANG)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,/STACK:20971520")
  endif()
endif()
```

## Best Practices

1. **Always test with debug tools in development**
2. **Use RAII and smart pointers to avoid manual memory management**
3. **Enable CRT debug heap in debug builds**
4. **Run with Application Verifier during testing**
5. **Use Page Heap for hard-to-reproduce corruption issues**
6. **Set up child process debugging for multi-process applications**
7. **Keep symbols available for meaningful stack traces**

## Troubleshooting

### High Memory Usage
- Page Heap can use significant memory, especially with `/full`
- Consider using normal page heap or `/random` percentage
- Increase virtual memory if needed

### Performance Impact
- Debug tools significantly slow execution
- Disable in production builds
- Use only during development and testing

### Symbol Loading Issues
- Ensure PDB files are available
- Set `_NT_SYMBOL_PATH` environment variable
- Use Microsoft Symbol Server: `srv*c:\symbols*https://msdl.microsoft.com/download/symbols`

### Child Process Not Attaching
- Verify extensions are installed and enabled
- Check that child executable name matches exactly
- Try manual attachment as fallback
- Ensure both parent and child have debug symbols
