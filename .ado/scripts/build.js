// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

//
// Script to build Hermes for Windows.
// See the options and printHelp() below for the usage details.
//
// This script is intended to be run in the Azure DevOps pipeline
// or locally with Node.js.
// It enables the following features:
// 1. Clean the previous configuration and built files.
// 2. Configure CMake before the build.
// 3. Build binaries.
// 4. Run tests.
// 5. Create NuGet packages.
//

import { execSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { parseArgs } from "node:util";
import { fileURLToPath } from "node:url";

// ES modules don't have __dirname, so we need to create it
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// The root of the local Hermes repository.
const sourcesPath = path.resolve(__dirname, path.join("..", ".."));

// The command line options.
const options = {
  help: { type: "boolean", default: false },
  configure: { type: "boolean", default: false },
  build: { type: "boolean", default: true },
  targets: { type: "string", multiple: true, default: [] },
  test: { type: "boolean", default: false },
  jstest: { type: "boolean", default: false },
  pack: { type: "boolean", default: false },
  "clean-all": { type: "boolean", default: false },
  "clean-build": { type: "boolean", default: false },
  "clean-tools": { type: "boolean", default: false },
  "clean-pkg": { type: "boolean", default: false },
  uwp: { type: "boolean", default: false },
  msvc: { type: "boolean", default: false },
  platform: {
    type: "string",
    multiple: true,
    default: ["x64"],
    validSet: ["x64", "x86", "arm64", "arm64ec"],
  },
  configuration: {
    type: "string",
    multiple: true,
    default: ["release"],
    validSet: ["debug", "release"],
  },
  "output-path": {
    type: "string",
    default: path.join(sourcesPath, "out"),
  },
  "semantic-version": { type: "string", default: "0.0.0" },
  "file-version": { type: "string", default: "0.0.0.0" },
  "windows-sdk-version": { type: "string", default: "" },
  "fake-build": { type: "boolean", default: false },
};

// To access parsed args values, use args.<option-name>.
const { values: args } = parseArgs({ options, allowNegative: true });

// To be used in help message.
const scriptRelativePath = path.relative(process.cwd(), __filename);

if (args.help) {
  printHelp();
  process.exit(0);
}

function printHelp() {
  console.log(`Usage: node ${scriptRelativePath} [options]

Note: All boolean flags support negation (e.g., --no-build, --no-test)
      CMake will be configured automatically if build folder is empty and --build is used

Options:
  --help                  Show this help message and exit
  --configure             Configure CMake before the build (default: ${
    options.configure.default
  })
  --build                 Build binaries (default: ${options.build.default})
  --no-build              Skip building binaries
  --targets               CMake build target(s) (default: all targets)
                          Common targets: hermesc, libshared, hermes, APITests
  --test                  Run tests (default: ${options.test.default})
  --jstest                Run JS regression tests (default: ${options.jstest.default})
  --pack                  Create NuGet packages (default: ${
    options.pack.default
  })
  --clean-all             Delete the whole output folder (default: ${
    options["clean-all"].default
  })
  --clean-build           Delete the build folder for the targeted configurations (default: ${
    options["clean-build"].default
  })
  --clean-tools           Delete the tools folder used for cross-platform builds (default: ${
    options["clean-tools"].default
  })
  --clean-pkg             Delete NuGet pkg and pkg-staging folders (default: ${
    options["clean-pkg"].default
  })
  --msvc                  Use MSVC compiler instead of Clang (default: ${
    options.msvc.default
  })
                          [Note: ARM64EC temporarily uses MSVC due to Clang 19.x issue]
  --uwp                   Build for UWP instead of Win32 (default: ${
    options.uwp.default
  })
  --platform              Target platform(s) (default: ${options.platform.default.join(
    ", ",
  )}) [valid values: ${options.platform.validSet.join(", ")}]
  --configuration         Build configuration(s) (default: ${options.configuration.default.join(
    ", ",
  )}) [valid values: ${options.configuration.validSet.join(", ")}]
  --output-path           Path to the output directory (default: ${
    options["output-path"].default
  })
  --semantic-version      NuGet package semantic version (default: ${
    options["semantic-version"].default
  })
  --file-version          Version set in binary files (default: ${
    options["file-version"].default
  })
  --windows-sdk-version   Windows SDK version (e.g., "10.0.19041.0") (default: ${
    options["windows-sdk-version"].default
  })
  --fake-build            Replace binaries with fake files for script debugging (default: ${
    options["fake-build"].default
  })
                          [Note: This skips actual building and cannot be used to test builds]

Examples:
  node ${scriptRelativePath} --configure --no-build        # Configure only, don't build
  node ${scriptRelativePath}                               # Build (will configure if needed)
  node ${scriptRelativePath} --platform arm64 --uwp        # Build ARM64 UWP
  node ${scriptRelativePath} --clean-build --msvc          # Clean and build with MSVC
  node ${scriptRelativePath} --targets hermesc             # Build only hermesc target
  node ${scriptRelativePath} --targets hermesc,libshared   # Build multiple targets
`);
}

// Convert all string arg values to lower case.
for (const [key, value] of Object.entries(args)) {
  if (typeof value === "string") {
    args[key] = value.toLowerCase();
  }
}

// Validate args values against the validSet.
for (const [key, value] of Object.entries(args)) {
  if (options[key]?.validSet) {
    const values = options[key].multiple ? value : [value];
    for (const item of values) {
      if (!options[key].validSet.includes(item)) {
        console.error(`Invalid value for ${key}: ${item}`);
        console.error(`Valid values are: ${options[key].validSet.join(", ")}`);
        process.exit(1);
      }
    }
  }
}

// Get the path to vcvarsall.bat.
// It is used to invoke CMake commands in the targeted MSVC context.
const vcVarsAllBat = getVCVarsAllBat();

main();

function main() {
  const startTime = new Date();

  ensureDir(args["output-path"]);
  args["output-path"] = path.resolve(args["output-path"]);

  // Force MSVC for ARM64EC due to Clang 19.x linker issues (LLVM #113658)
  // This must be removed when Clang 20 is available
  const useMsvc = args.msvc || args.platform.includes("arm64ec");

  args.hostCpuArch = getHostCpuArch();

  console.log();
  console.log(`The ${scriptRelativePath} is invoked with parameters:`);
  console.log(`            clean-pkg: ${args["clean-pkg"]}`);
  console.log(`            configure: ${args.configure}`);
  console.log(`                build: ${args.build}`);
  console.log(`              targets: ${args.targets}`);
  console.log(`                 test: ${args.test}`);
  console.log(`               jstest: ${args.jstest}`);
  console.log(`                 pack: ${args.pack}`);
  console.log(`            clean-all: ${args["clean-all"]}`);
  console.log(`          clean-build: ${args["clean-build"]}`);
  console.log(`          clean-tools: ${args["clean-tools"]}`);
  console.log(`            clean-pkg: ${args["clean-pkg"]}`);
  console.log(`                 msvc: ${args.msvc}`);
  console.log(`              useMsvc: ${useMsvc}`);
  console.log(`                  uwp: ${args.uwp}`);
  console.log(`             platform: ${args.platform}`);
  console.log(`        configuration: ${args.configuration}`);
  console.log(`    host architecture: ${args.hostCpuArch}`);
  console.log(`          output-path: ${args["output-path"]}`);
  console.log(`     semantic-version: ${args["semantic-version"]}`);
  console.log(`         file-version: ${args["file-version"]}`);
  console.log(`  windows-sdk-version: ${args["windows-sdk-version"]}`);
  console.log(`           fake-build: ${args["fake-build"]}`);
  console.log();

  removeUnusedFilesForComponentGovernance();

  updateHermesVersion();

  if (args["clean-all"]) {
    cleanAll();
  }

  const runParams = {
    isUwp: args.uwp,
    buildPath: path.join(args["output-path"], "build"),
    toolsPath: path.join(args["output-path"], "tools"),
    pkgStagingPath: path.join(args["output-path"], "pkg-staging"),
    pkgPath: path.join(args["output-path"], "pkg"),
  };

  if (args["clean-tools"]) {
    cleanTools(runParams);
  }

  if (args["clean-pkg"]) {
    cleanPkg(runParams);
  }

  const platforms = Array.isArray(args.platform)
    ? args.platform
    : [args.platform];
  const configurations = Array.isArray(args.configuration)
    ? args.configuration
    : [args.configuration];
  platforms.forEach((platform) => {
    configurations.forEach((configuration) => {
      const configParams = {
        ...runParams,
        platform,
        hostCpuArch: args.hostCpuArch,
        configuration,
      };
      const buildParams = {
        ...configParams,
        msvc: useMsvc,
        buildPath: getBuildPath(configParams),
        hasCustomTargets: args.targets.length > 0,
        targets: getTargets(args.targets, configParams),
        onBuildCompleted: copyBuiltFilesToPkgStaging,
      };

      console.log("buildParams: ", buildParams);

      if (args["fake-build"]) {
        copyFakeFilesToPkgStaging(buildParams);
        return;
      }
      if (args["clean-build"]) {
        cleanBuild(buildParams);
      }
      if (args.configure) {
        cmakeConfigure(buildParams);
      }
      if (args.build) {
        cmakeBuild(buildParams);
      }
      if (args.test) {
        cmakeTest(buildParams);
      }
      if (args.jstest) {
        cmakeJSTest(buildParams);
      }
    });
  });

  if (args.pack) {
    packNuGet(runParams);
  }

  const elapsedTime = new Date() - startTime;
  const totalTime = new Date(elapsedTime).toISOString().substring(11, 19);
  console.log(`Build took ${totalTime} to run`);
  console.log();
}

function getHostCpuArch() {
  const arch = process.arch;
  if (arch === "x64" || arch === "arm64") {
    return arch;
  }
  throw new Error(`Unsupported host CPU architecture: ${arch}`);
}

function removeUnusedFilesForComponentGovernance() {
  if (args["file-version"].trim() === "0.0.0.0") {
    return;
  }
  deleteDir(path.join(sourcesPath, "unsupported", "juno"));
}

function updateHermesVersion() {
  if (!args["semantic-version"].trim()) {
    return;
  }
  if (args["file-version"].trim() === "0.0.0.0") {
    return;
  }

  let hermesVersion = args["semantic-version"];
  if (hermesVersion.startsWith("0.0.0")) {
    hermesVersion = args["file-version"];
  }

  const cmakeListsPath = path.join(sourcesPath, "CMakeLists.txt");
  const cmakeContent = fs
    .readFileSync(cmakeListsPath, "utf8")
    .replace("VERSION 0.12.0", `VERSION ${hermesVersion}`);
  fs.writeFileSync(cmakeListsPath, cmakeContent);

  const packageJsonPath = path.join(sourcesPath, "npm", "package.json");
  const packageContent = fs
    .readFileSync(packageJsonPath, "utf8")
    .replace(/"version": ".*",/, `"version": "${args["semantic-version"]}",`);
  fs.writeFileSync(packageJsonPath, packageContent);

  console.log(`Semantic version set to ${args["semantic-version"]}`);
  console.log(`Hermes version set to ${hermesVersion}`);
}

function getBuildPath({ isUwp, buildPath, platform, configuration }) {
  const triplet = `${getAppPlatformName(isUwp)}-${platform}-${configuration}`;
  return path.join(buildPath, triplet);
}

function getTargets(userTargets, configParams) {
  // If user specified targets, use those
  if (userTargets.length > 0) {
    const targetList = [];

    for (const target of userTargets) {
      if (typeof target === "string") {
        // Split comma-separated values and add to list
        targetList.push(
          ...target
            .split(",")
            .map((t) => t.trim())
            .filter((t) => t),
        );
      }
    }
    // Remove duplicates and join with spaces
    return [...new Set(targetList)].join(" ");
  }

  // If no user targets are specified, fall back to the existing logic
  return isCrossPlatformBuild(configParams) ? "libshared" : "";
}

function getAppPlatformName(isUwp) {
  return isUwp ? "uwp" : "win32";
}

function getCMakePreset({ msvc, configuration }) {
  const compiler = msvc ? "msvc" : "clang";
  const configType = configuration === "release" ? "release" : "debug";
  return `ninja-${compiler}-${configType}`;
}

function cleanAll() {
  deleteDir(args["output-path"]);
}

function cleanTools(runParams) {
  deleteDir(runParams.toolsPath);
}

function cleanBuild(buildParams) {
  deleteDir(buildParams.buildPath);
}

function cleanPkg(runParams) {
  deleteDir(runParams.pkgStagingPath);
  deleteDir(runParams.pkgPath);
}

function cmakeConfigure(buildParams) {
  const { msvc, isUwp, platform, hostCpuArch, toolsPath } = buildParams;

  console.log("buildParams: ", buildParams);
  cmakeBuildHermesCompiler(buildParams);

  const preset = getCMakePreset(buildParams);
  console.log(`Using CMake preset: ${preset}`);

  const genArgs = [`--preset=${preset}`, `-B"${buildParams.buildPath}"`];

  if (args["file-version"] && args["file-version"] !== "0.0.0.0") {
    genArgs.push(`-DHERMES_FILE_VERSION=${args["file-version"]}`);
  }

  // Add cross-compilation target for non-host platforms when using Clang
  if (platform !== hostCpuArch && !msvc) {
    let targetTriple = "";
    if (platform === "x86") {
      targetTriple = "i686-pc-windows-msvc";
    } else if (platform === "x64") {
      targetTriple = "x64-pc-windows-msvc";
    } else if (platform === "arm64") {
      targetTriple = "aarch64-pc-windows-msvc";
    } else if (platform === "arm64ec") {
      targetTriple = "arm64ec-pc-windows-msvc";
    }

    if (targetTriple) {
      genArgs.push(`-DCMAKE_C_FLAGS="-target ${targetTriple}"`);
      genArgs.push(`-DCMAKE_CXX_FLAGS="-target ${targetTriple}"`);
    }
  }

  genArgs.push(`-DHERMES_WINDOWS_TARGET_PLATFORM=${platform}`);

  if (isUwp) {
    genArgs.push("-DCMAKE_SYSTEM_NAME=WindowsStore");
    genArgs.push(`-DCMAKE_SYSTEM_VERSION="${args["windows-sdk-version"]}"`);

    if (!msvc) {
      genArgs.push('-DCMAKE_EXE_LINKER_FLAGS="-Wl,/APPCONTAINER -lwindowsapp"');
      genArgs.push(
        '-DCMAKE_SHARED_LINKER_FLAGS="-Wl,/APPCONTAINER -lwindowsapp"',
      );
    }
  }

  // Use prebuilt Hermes compiler for cross-platform builds
  if (isCrossPlatformBuild(buildParams)) {
    genArgs.push(
      `-DIMPORT_HOST_COMPILERS="${path.join(toolsPath, "ImportHostCompilers.cmake")}"`,
    );
  }

  // We must force the native build mostly for the Clang compiler.
  genArgs.push(
    `-DHERMES_WINDOWS_FORCE_NATIVE_BUILD=${
      isCrossPlatformBuild(buildParams) ? "OFF" : "ON"
    }`,
  );

  runCMakeCommand(`cmake ${genArgs.join(" ")} "${sourcesPath}"`, buildParams);
}

function cmakeBuild(buildParams) {
  const { buildPath, targets, onBuildCompleted } = buildParams;
  if (!fs.existsSync(buildPath)) {
    cmakeConfigure(buildParams);
  }

  cmakeBuildHermesCompiler(buildParams);

  const target = targets ? `--target ${targets}` : "";
  runCMakeCommand(`cmake --build . ${target}`, buildParams);

  if (onBuildCompleted) {
    onBuildCompleted(buildParams);
  }
}

function cmakeTest(buildParams) {
  if (isCrossPlatformBuild(buildParams)) {
    console.log("Skip testing for cross-platform builds");
    return;
  }

  if (!fs.existsSync(buildParams.buildPath)) {
    cmakeBuild(buildParams);
  }

  runCMakeCommand("ctest --output-on-failure", buildParams);
}

// Run JS tests via check-hermes target using buildParams
function cmakeJSTest(buildParams) {
  setupJSTestEnvPaths();
  runCMakeCommand("cmake --build . --target check-hermes", buildParams);
}

function cmakeBuildHermesCompiler(buildParams) {
  const { toolsPath } = buildParams;

  // Only build Hermes compiler for cross-platform builds.
  if (!isCrossPlatformBuild(buildParams)) {
    return;
  }

  const hermesCompilerPath = path.join(toolsPath, "bin", "hermesc.exe");
  if (!fs.existsSync(hermesCompilerPath)) {
    cmakeBuild({
      ...buildParams,
      isUwp: false,
      platform: args.hostCpuArch,
      hostCpuArch: args.hostCpuArch,
      configuration: "release",
      msvc: args.msvc,
      buildPath: toolsPath,
      targets: "hermesc",
      onBuildCompleted: undefined,
    });
  }
}

function runCMakeCommand(command, buildParams) {
  const { platform, buildPath } = buildParams;

  const env = { ...process.env };
  if (platform === "arm64ec") {
    env.CFLAGS = "-arm64EC";
    env.CXXFLAGS = "-arm64EC";
  }

  ensureDir(buildPath);
  const originalCwd = process.cwd();
  process.chdir(buildPath);
  console.log(`Changed CWD to: ${buildPath}`);
  try {
    const vsCommand =
      `"${vcVarsAllBat}" ${getVCVarsAllBatArgs(buildParams)}` +
      ` && ${command} 2>&1`;
    console.log(`Run command: ${vsCommand}`);
    execSync(vsCommand, { stdio: "inherit", env });
  } catch (error) {
    // Don't show JavaScript stack trace for build tool errors
    // The actual error output has already been displayed via stdio: "inherit"
    console.error(
      `\nBuild command failed with exit code: ${error.status || "unknown"}`,
    );
    process.exit(error.status || 1);
  } finally {
    process.chdir(originalCwd);
    console.log(`Changed CWD back to: ${originalCwd}`);
  }
}

function isCrossPlatformBuild({ isUwp, hostCpuArch, platform }) {
  // Return true if we either build for UWP or the host architecture does
  // not match the target architecture.
  // x86 can run natively on x64 machines, so it's not considered cross-platform.
  // In true cross-platform cases we must build host specific tools and cannot run unit tests.
  return (
    isUwp ||
    (hostCpuArch === "x64" && platform.startsWith("arm64")) ||
    (hostCpuArch === "arm64" && platform !== "arm64")
  );
}

function copyBuiltFilesToPkgStaging(buildParams) {
  const { buildPath, hasCustomTargets } = buildParams;
  const { dllStagingPath, toolsStagingPath } = ensureStagingPaths(buildParams);

  const copyFileIsOptional = hasCustomTargets;
  if (copyFileIsOptional) {
    console.log("Optional file copying mode enabled");
  }

  const dllSourcePath = path.join(buildPath, "API", "hermes_shared");
  copyFile("hermes.dll", dllSourcePath, dllStagingPath, copyFileIsOptional);
  copyFile("hermes.lib", dllSourcePath, dllStagingPath, copyFileIsOptional);
  copyFile("hermes.pdb", dllSourcePath, dllStagingPath, copyFileIsOptional);

  if (!isCrossPlatformBuild(buildParams)) {
    const toolsSourcePath = path.join(buildPath, "bin");
    copyFile(
      "hermes.exe",
      toolsSourcePath,
      toolsStagingPath,
      copyFileIsOptional,
    );
    copyFile(
      "hermesc.exe",
      toolsSourcePath,
      toolsStagingPath,
      copyFileIsOptional,
    );
  }
}

function copyFakeFilesToPkgStaging(buildParams) {
  const { dllStagingPath, toolsStagingPath } = ensureStagingPaths(buildParams);

  createFakeBinFile(dllStagingPath, "hermes.dll");
  createFakeBinFile(dllStagingPath, "hermes.lib");
  createFakeBinFile(dllStagingPath, "hermes.pdb");

  if (!isCrossPlatformBuild(buildParams)) {
    createFakeBinFile(toolsStagingPath, "hermes.exe");
    createFakeBinFile(toolsStagingPath, "hermesc.exe");
  }
}

function copyFile(fileName, sourcePath, targetPath, optional = false) {
  ensureDir(targetPath);
  const sourceFile = path.join(sourcePath, fileName);
  const targetFile = path.join(targetPath, fileName);

  if (optional && !fs.existsSync(sourceFile)) {
    console.log(`Skipping copy of ${fileName} (file not found, optional copy)`);
    return;
  }

  fs.copyFileSync(sourceFile, targetFile);
}

function createFakeBinFile(targetPath, fileName) {
  fs.copyFileSync(
    path.join(process.env.SystemRoot, "system32", "kernel32.dll"),
    path.join(targetPath, fileName),
  );
}

function ensureStagingPaths(buildParams) {
  const { isUwp, platform, configuration } = buildParams;

  // Ensure that both Win32 and UWP staging paths exist to enable Nuget pack
  // working even if we have a single DLL.
  const win32DllStagingPath = path.join(
    buildParams.pkgStagingPath,
    "lib",
    "native",
    getAppPlatformName(/*isUwp:*/ false),
    configuration,
    platform,
  );
  const uwpDllStagingPath = path.join(
    buildParams.pkgStagingPath,
    "lib",
    "native",
    getAppPlatformName(/*isUwp:*/ true),
    configuration,
    platform,
  );
  ensureDir(win32DllStagingPath);
  ensureDir(uwpDllStagingPath);

  const dllStagingPath = isUwp ? uwpDllStagingPath : win32DllStagingPath;

  const toolsStagingPath = path.join(
    buildParams.pkgStagingPath,
    "tools",
    "native",
    configuration,
    platform,
  );
  ensureDir(toolsStagingPath);

  return { dllStagingPath, toolsStagingPath };
}

function packNuGet(runParams) {
  const { pkgStagingPath, pkgPath } = runParams;

  const hermesApiPath = path.join(sourcesPath, "API");
  const stagingBuildNativePath = path.join(pkgStagingPath, "build", "native");
  const stagingIncludePath = path.join(stagingBuildNativePath, "include");

  // Copy jsi headers
  const stagingJsiPath = path.join(stagingIncludePath, "jsi");
  ensureDir(stagingJsiPath);
  fs.cpSync(path.join(hermesApiPath, "jsi", "jsi"), stagingJsiPath, {
    recursive: true,
  });

  // Copy Node API headers
  const hermesNodeApiPath = path.join(hermesApiPath, "hermes_node_api");
  const nodeApiPath = path.join(hermesNodeApiPath, "node_api");
  const stagingNodeApiPath = path.join(stagingIncludePath, "node-api");
  ensureDir(stagingNodeApiPath);
  copyFile("js_native_api.h", nodeApiPath, stagingNodeApiPath);
  copyFile("js_native_api_types.h", nodeApiPath, stagingNodeApiPath);
  copyFile("node_api.h", nodeApiPath, stagingNodeApiPath);
  copyFile("node_api_types.h", nodeApiPath, stagingNodeApiPath);

  // Copy Hermes API headers
  const hermesSharedApiPath = path.join(hermesApiPath, "hermes_shared");
  const stagingHermesApiPath = path.join(stagingIncludePath, "hermes");
  ensureDir(stagingHermesApiPath);
  copyFile("js_runtime_api.h", hermesSharedApiPath, stagingHermesApiPath);
  copyFile("hermes_api.h", hermesSharedApiPath, stagingHermesApiPath);

  // Copy license files
  const nugetSourcePath = path.join(sourcesPath, ".ado", "Nuget");
  const stagingLicensePath = path.join(pkgStagingPath, "license");
  ensureDir(stagingLicensePath);
  copyFile("LICENSE", sourcesPath, stagingLicensePath);
  copyFile("NOTICE.txt", nugetSourcePath, stagingLicensePath);

  // Copy MSBuild files
  ensureDir(path.join(pkgStagingPath, "build", "native"));
  copyFile(
    "Microsoft.JavaScript.Hermes.props",
    nugetSourcePath,
    stagingBuildNativePath,
  );
  copyFile(
    "Microsoft.JavaScript.Hermes.targets",
    nugetSourcePath,
    stagingBuildNativePath,
  );
  copyFile(
    "Microsoft.JavaScript.Hermes.nuspec",
    nugetSourcePath,
    pkgStagingPath,
  );

  // Copy UAP tag file
  const stagingUapPath = path.join(pkgStagingPath, "lib", "uap");
  if (!fs.existsSync(stagingUapPath)) {
    ensureDir(stagingUapPath);
    fs.writeFileSync(path.join(stagingUapPath, "_._"), "");
  }

  ensureDir(pkgPath);

  const packageVersion = args["semantic-version"];
  const repoUrl = "https://github.com/microsoft/hermes-windows";
  const repoBranch = execSync("git rev-parse --abbrev-ref HEAD")
    .toString()
    .trim();
  const repoCommit = execSync("git rev-parse HEAD").toString().trim();
  const baseProperties =
    `nugetroot=${pkgStagingPath}` +
    `;version=${packageVersion}` +
    `;repoUrl=${repoUrl}` +
    `;repoBranch=${repoBranch}` +
    `;repoCommit=${repoCommit}`;
  const packageProperties = `${baseProperties};fat_suffix=;exclude_bin_files=**/*.pdb`;
  const fatPackageProperties = `${baseProperties};fat_suffix=.Fat;exclude_bin_files=*.txt`;

  const nugetPackBaseCmd = `nuget pack "${path.join(
    pkgStagingPath,
    "Microsoft.JavaScript.Hermes.nuspec",
  )}" -OutputDirectory "${pkgPath}" -NoDefaultExcludes`;
  const nugetPackCmd = `${nugetPackBaseCmd} -Properties "${packageProperties}"`;
  console.log(`Run command: ${nugetPackCmd}`);
  execSync(nugetPackCmd, { stdio: "inherit" });

  const fatNugetPackCmd = `${nugetPackBaseCmd} -Properties "${fatPackageProperties}"`;
  console.log(`Run command: ${fatNugetPackCmd}`);
  execSync(fatNugetPackCmd, { stdio: "inherit" });
}

function getVCVarsAllBat() {
  const vsWhere = path.join(
    process.env["ProgramFiles(x86)"] || process.env["ProgramFiles"],
    "Microsoft Visual Studio",
    "Installer",
    "vswhere.exe",
  );
  if (!fs.existsSync(vsWhere)) {
    throw new Error("Could not find vswhere.exe");
  }

  const versionJson = JSON.parse(
    execSync(`"${vsWhere}" -format json -version 17`).toString(),
  );
  if (versionJson.length > 1) {
    console.warn("More than one VS install detected, picking the first one");
  }

  const installationPath = versionJson[0].installationPath;
  const vcVarsAllBat = path.join(
    installationPath,
    "VC",
    "Auxiliary",
    "Build",
    "vcvarsall.bat",
  );
  if (!fs.existsSync(vcVarsAllBat)) {
    throw new Error(
      `Could not find vcvarsall.bat at expected Visual Studio installation path: ${vcVarsAllBat}`,
    );
  }

  return vcVarsAllBat;
}

function getVCVarsAllBatArgs(buildParams) {
  const { platform, isUwp, hostCpuArch } = buildParams;
  let vcArgs = "";
  if (hostCpuArch === "x64") {
    switch (platform) {
      case "x86":
        vcArgs += " amd64_x86";
        break;
      case "arm64":
      case "arm64ec":
        vcArgs += " amd64_arm64";
        break;
      case "x64":
      default:
        vcArgs += " amd64";
        break;
    }
  } else if (hostCpuArch === "arm64") {
    switch (platform) {
      case "x86":
        vcArgs += " arm64_x86";
        break;
      case "x64":
        vcArgs += " arm64_amd64";
        break;
      case "arm64":
      default:
        vcArgs += " arm64";
        break;
    }
  }

  if (isUwp) {
    vcArgs += " uwp";
  }

  if (args["windows-sdk-version"]) {
    vcArgs += ` ${args["windows-sdk-version"]}`;
  }

  vcArgs += " -vcvars_spectre_libs=spectre";

  return vcArgs;
}

function ensureDir(dirPath) {
  if (!fs.existsSync(dirPath)) {
    fs.mkdirSync(dirPath, { recursive: true });
  }
}

function deleteDir(dirPath) {
  if (fs.existsSync(dirPath)) {
    fs.rmSync(dirPath, { recursive: true, force: true });
  }
}

function setupJSTestEnvPaths() {
  console.log("Setting up environment paths...");

  // Helper function to find executable path using 'where'
  function findExecutable(name) {
    try {
      // The 'where' command returns a list of paths, one per line. We want the first one.
      const output = execSync(`where ${name}`, { encoding: "utf8" });
      const firstPath = output.split("\r\n")[0];
      return firstPath ? path.dirname(firstPath) : null;
    } catch {
      return null;
    }
  }

  function showWarning(message) {
    console.warn(`Warning: ${message}`);
  }

  // Add Git Bash to PATH for LIT tests
  (function () {
    const gitDir = findExecutable("git.exe");
    if (!gitDir) {
      return showWarning("Git (git.exe) not found in PATH.");
    }
    console.log(`Found Git at: ${gitDir}`);
    const gitBashDir = gitDir.replace("cmd", "bin");
    if (!fs.existsSync(path.join(gitBashDir, "bash.exe"))) {
      return showWarning(`Git Bash (bash.exe) not found at: ${gitBashDir}`);
    }
    if (!process.env.PATH.includes(gitBashDir)) {
      console.log(`Adding Git Bash directory to PATH: ${gitBashDir}`);
      process.env.PATH = `${gitBashDir};${process.env.PATH}`;
    }
  })();

  // Add Python to PATH
  (function () {
    const pythonDir = findExecutable("python.exe");
    if (!pythonDir) {
      return showWarning("Python (python.exe) not found in PATH.");
    }
    const pythonScriptsDir = path.join(pythonDir, "Scripts");
    console.log(`Found Python at: ${pythonDir}`);
    if (!process.env.PATH.includes(pythonDir)) {
      console.log(
        `Adding python directories to PATH: ${pythonDir}, ${pythonScriptsDir}`,
      );
      process.env.PATH = `${pythonDir};${pythonScriptsDir};${process.env.PATH}`;
    }
  })();

  // Add the bash-alias directory to PATH for the python3 wrapper
  (function () {
    const aliasDir = path.join(__dirname, "bash-alias");
    if (!fs.existsSync(path.join(aliasDir, "python3"))) {
      return showWarning(`python3 alias script not found at: ${aliasDir}`);
    }
    if (!process.env.PATH.includes(aliasDir)) {
      console.log(`Adding python3 alias directory to PATH: ${aliasDir}`);
      process.env.PATH = `${aliasDir};${process.env.PATH}`;
    }
  })();

  console.log("Environment setup complete.");
}
