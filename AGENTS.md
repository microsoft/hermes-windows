# AGENTS.md

This file provides guidance to coding agents working in this repository.

## Environment and Shell

- This repository may be used from Windows, macOS, or Linux. Use the native
  shell for the current environment: PowerShell on Windows and the configured
  POSIX shell on macOS or Linux.
- Prefer portable Coreutils commands and pipelines when they express the task
  clearly. Use options supported on every target platform involved; macOS and
  GNU utilities can differ in less common flags.
- Windows development machines are expected to have
  [Microsoft Coreutils](https://github.com/microsoft/coreutils) installed. Before
  relying on it, verify the installation and the executable you intend to use:

```powershell
Get-Command coreutils-manager -CommandType Application
Get-Command grep -CommandType Application
```

  If `coreutils-manager` is missing, report it. The documented installation
  command is `winget install Microsoft.Coreutils`.
- PowerShell aliases can shadow Coreutils executables. When resolution matters,
  use `Get-Command <name> -All` and select the application, or invoke the `.exe`
  name explicitly. Coreutils provides portable programs, not POSIX shell syntax;
  continue to use PowerShell quoting, escaping, variables, and control flow on
  Windows.
- Run repository commands from the repository root and pass explicit paths when
  practical. Do not leave the persistent terminal in a different directory.
- When a tool must run from another directory, scope the location change using
  syntax appropriate to the current shell. For example, in PowerShell:

```powershell
Push-Location tools\fork-sync
try {
  npm test
} finally {
  Pop-Location
}
```

  In a POSIX shell:

```sh
(cd tools/fork-sync && npm test)
```

- Prefer workspace file tools for searching, reading, and editing files. Use the
  terminal for builds, tests, version control, and commands that need the shell.

## Working Practices

- Inspect the relevant code and nearby tests before editing. Follow existing
  patterns and keep changes focused on the requested behavior.
- Preserve unrelated working-tree changes. Do not clean, reset, restore, or
  rewrite user changes unless explicitly asked.
- Do not create commits, push branches, or open pull requests unless explicitly
  requested.
- After a material change, run the narrowest relevant build or test first, then
  broaden validation when the change warrants it. Report failures and skipped
  validation explicitly.

## Overview

This is the **hermes-windows** fork of facebook/hermes, adapted for Windows.
Hermes is a JavaScript engine optimized for fast start-up of React Native apps.
It features ahead-of-time static optimization and compact bytecode.

The fork syncs with the upstream `static_h` branch.

## Build Commands

Do not run CMake, Ninja, or CTest directly for Windows builds. The build
environment requires Visual Studio variables configured by the repository build
script. Use the `.\dev` PowerShell wrapper from the repository root.

### PowerShell Wrapper (preferred)

```powershell
.\dev build --platform x64 --configuration debug
.\dev build --platform arm64ec --configuration release --binskim
```

### Available Platforms

- `x64` (default) — uses Clang
- `x86` — uses Clang
- `arm64` — uses Clang
- `arm64ec` — uses MSVC (Clang not supported yet)

### Build Options

```powershell
# Build specific targets
.\dev build --targets hermes_rt,hermes-icu --configuration debug

# Run C++ tests
.\dev build --test --configuration debug

# Run lit tests (JS regression tests)
.\dev build --jstest --configuration debug

# Run Test262 intl tests
.\dev build --test262-intl --configuration debug

# Run BinSkim security validation
.\dev build --no-build --platform arm64ec --configuration release --binskim

# Configure only (no build)
.\dev build --configure --no-build --platform x64

# Clean build
.\dev build --clean-build --platform x64
```

### Build Output

Build output goes to: `out/build/win32-<platform>-<config>/`

### Build Script

The main build script is `.ado/scripts/build.js`. It handles:
- Visual Studio environment setup via `vcvarsall.bat`
- CMake configuration with correct flags per platform
- Building, testing, packaging, and BinSkim validation

## Code Architecture

The build produces two VM variants: "regular" (full VM with compiler) and "lean" (excludes parser and compiler for smaller binary size).

### Core Components

- **lib/VM/**: Virtual machine core - runtime, interpreter, garbage collector, object model
- **lib/VM/JSLib/**: JavaScript standard library implemented in C++ (Array, Object, String, etc.)
- **lib/InternalJavaScript/**: JavaScript polyfills compiled into the VM (e.g., Math.sumPrecise, Promise)
- **lib/Parser/**: JavaScript parser
- **lib/AST/**: Abstract syntax tree definitions
- **lib/IR/**: Intermediate representation for optimization
- **lib/IRGen/**: Generates IR from AST
- **lib/BCGen/**: Bytecode generation from IR
- **lib/Sema/**: Semantic analysis
- **lib/Support/**: Shared utilities and data structures
- **include/hermes/**: Public header files organized by component
- **API/hermes/extensions/**: JSI-based runtime extensions (see Extensions section below)

### Key VM Files

- `lib/VM/Runtime.cpp`: Main runtime implementation
- `lib/VM/Interpreter.cpp`: Bytecode interpreter
- `lib/VM/Callable.cpp`: Function and callable object implementation
- `lib/VM/JSObject.cpp`: JavaScript object model
- `lib/VM/Operations.cpp`: Core JS operations (typeof, instanceof, etc.)
- `lib/VM/gcs/`: Garbage collector implementations

### Windows-Specific Files

- `cmake/modules/HermesWindows.cmake`: Windows build configuration (MSVC/Clang flags, security flags, linker flags)
- `.ado/scripts/build.js`: Main build orchestration script
- `dev.ps1`: PowerShell developer wrapper

### Testing

- `test/`: Lit-based integration tests organized by component
- `unittests/`: Google Test unit tests (VMRuntime, Support, API, Parser, etc.)

### Auto-Updating Tests

Some lit tests use `%FileCheckOrRegen` instead of `%FileCheck`. These are auto-updating tests whose expected output can be regenerated automatically.

If such a test fails due to intentional changes (e.g., changed output format, added runtime modules affecting IDs), and you understand why the failure occurred, rebuild with the `update-lit` target.

**Important:** Only use `update-lit` when you understand the cause of the failure. Review the changes to verify they match your expectations. Do not blindly regenerate tests to make them pass.

## Code Style

Key conventions:

- **C++17**, no exceptions or RTTI
- **Naming**: Classes (`PascalCase`), functions/methods (`camelCase`), variables (`camelCase`), member vars (`_suffix`), constants (`SNAKE_CASE` or `kCamelCase`)
- **structs**: PODs only; use `class` for anything with constructors/destructors
- **Line limit**: 80 characters, 2-space indent
- **Doc comments**: Required for every declaration
- **Inlining**: Only trivial one-line methods in class body

## Native Function Development

Standard signature for VM native functions:
```cpp
CallResult<HermesValue> funcName(void *context, Runtime &runtime)
```

Access arguments:
```cpp
NativeArgs args = runtime.getCurrentFrame().getNativeArgs();
```

## Macro-Generated Code

When making systematic changes, check for macro-generated code:
- `NATIVE_ERROR_TYPE` macro in `lib/VM/JSLib/Error.cpp` - generates error constructors
- `TYPED_ARRAY` macro in `lib/VM/JSLib/TypedArray.cpp` - generates typed array constructors
- `NATIVE_FUNCTION` macro in `include/hermes/VM/JSNativeFunctions.h` - generates function declarations

## GC Handle Patterns

### Locals Pattern (preferred for new code)
```cpp
struct : public Locals {
  PinnedValue<JSObject> objHandle;
  PinnedValue<PropertyAccessor> accessor;
  PinnedValue<> tempValue;  // untyped
} lv;
LocalsRAII lraii(runtime, &lv);

// Assignment from PseudoHandle
lv.value = std::move(pseudoHandle);

// Assignment from CallResult with known type
lv.obj.castAndSetHermesValue<JSObject>(callResult.getValue());
```

### Critical: Null Prototype Handling
```cpp
// When traversing prototype chains, always check for null:
if (!*protoRes) {
  lv.O = nullptr;
} else {
  lv.O.castAndSetHermesValue<JSObject>(protoRes->getHermesValue());
}
```

## Copyright Headers

Do not add or change a copyright header unless the task requires it. When a new
file needs a header, follow neighboring files in the same component: upstream
Hermes files generally use the Meta header, while Windows-specific files may use
the Microsoft header.

## Extensions System

Hermes supports JSI-based extensions that add runtime functionality. Extensions use the stable JSI API rather than internal Hermes APIs, making them easier to maintain.

### Directory Structure

- `API/hermes/extensions/` - Core extensions maintained by the Hermes team
- `API/hermes/extensions/contrib/` - Community-contributed extensions

### Adding Extensions

Each extension consists of:
1. A JavaScript file (`NN-ExtensionName.js`) with a setup function
2. C++ files (`ExtensionName.h/cpp`) that call the JS setup and optionally provide native helpers

See `API/hermes/extensions/README.md` for detailed instructions.

### Contrib Extensions

Community contributions go in `extensions/contrib/`. These are:
- Maintained by contributors, not the Hermes team
- Enabled by default, but can be disabled with `-DHERMES_ENABLE_CONTRIB_EXTENSIONS=OFF`
- May be promoted to core if widely adopted

See `API/hermes/extensions/contrib/README.md` for contributor guidelines.

## Benchmarks

See `benchmarks/hermes-windows/README.md` for full documentation on running benchmarks, available categories, standalone benchmark files, and before/after comparison workflow.
