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

const { execSync } = require("child_process");
const fs = require("fs");
const path = require("path");
const { parseArgs } = require("node:util");

// The root of the local Hermes repository.
const sourcesPath = path.resolve(__dirname, path.join("..", ".."));

// The command line options.
const options = {
  help: { type: "boolean", default: false },
  configure: { type: "boolean", default: false },
  build: { type: "boolean", default: true },
  test: { type: "boolean", default: false },
  pack: { type: "boolean", default: false },
  "clean-all": { type: "boolean", default: false },
  "clean-build": { type: "boolean", default: false },
  "clean-tools": { type: "boolean", default: false },
  "clean-pkg": { type: "boolean", default: false },
  uwp: { type: "boolean", default: false },
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

Options:
  --help                  Show this help message and exit
  --configure             Configure CMake before the build (default: ${
    options.configure.default
  })
  --build                 Build binaries (default: ${options.build.default})
  --test                  Run tests (default: ${options.test.default})
  --pack                  Create NuGet packages (default: ${
    options.pack.default
  })
  --clean-all             Delete the whole output folder (default: ${
    options["clean-all"].default
  })
  --clean-build           Delete the build folder for the targeted configurations (default: ${
    options["clean-build"].default
  })
  --clean-tools           Delete the tools folder used for UWP and ARM64 builds (default: ${
    options["clean-tools"].default
  })
  --clean-pkg             Delete NuGet pkg and pkg-staging folders (default: ${
    options["clean-pkg"].default
  })
  --uwp                   Should we build for UWP or Win32? (default: ${
    options.uwp.default
  })
  --platform              Target platform(s) (default: ${options.platform.default.join(
    ", "
  )}) [valid values: ${options.platform.validSet.join(", ")}]
  --configuration         Build configuration(s) (default: ${options.configuration.default.join(
    ", "
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

  console.log();
  console.log(`The ${scriptRelativePath} is invoked with parameters:`);
  console.log(`            clean-pkg: ${args["clean-pkg"]}`);
  console.log(`            configure: ${args.configure}`);
  console.log(`                build: ${args.build}`);
  console.log(`                 test: ${args.test}`);
  console.log(`                 pack: ${args.pack}`);
  console.log(`            clean-all: ${args["clean-all"]}`);
  console.log(`          clean-build: ${args["clean-build"]}`);
  console.log(`          clean-tools: ${args["clean-tools"]}`);
  console.log(`            clean-pkg: ${args["clean-pkg"]}`);
  console.log(`                  uwp: ${args.uwp}`);
  console.log(`             platform: ${args.platform}`);
  console.log(`        configuration: ${args.configuration}`);
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
        configuration,
      };
      const buildParams = {
        ...configParams,
        buildPath: getBuildPath(configParams),
        targets: isCrossPlatformBuild(configParams) ? "libshared" : "",
        onBuildCompleted: copyBuiltFilesToPkgStaging,
      };
      console.log(
        "Build for " +
          `IsUWP: ${buildParams.isUwp}, ` +
          `Platform: ${buildParams.platform}, ` +
          `Configuration: ${buildParams.configuration}, ` +
          `Build path: ${buildParams.buildPath}`
      );

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

function getAppPlatformName(isUwp) {
  return isUwp ? "uwp" : "win32";
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
  const { isUwp, platform, configuration, toolsPath } = buildParams;

  cmakeBuildHermesCompiler(buildParams);

  const genArgs = ["-G Ninja"];

  const cmakeConfiguration =
    configuration === "release" ? "Release" : "FastDebug";
  genArgs.push(`-DCMAKE_BUILD_TYPE=${cmakeConfiguration}`);
  genArgs.push("-DHERMESVM_PLATFORM_LOGGING=ON");

  if (args["file-version"] && args["file-version"] !== "0.0.0.0") {
    genArgs.push(`-DHERMES_FILE_VERSION=${args["file-version"]}`);
  }

  genArgs.push("-DHERMES_ENABLE_DEBUGGER=ON");
  genArgs.push("-DHERMES_ENABLE_INTL=ON");

  genArgs.push(
    `-DHERMES_MSVC_USE_PLATFORM_UNICODE_WINGLOB=${isUwp ? "OFF" : "ON"}`
  );

  if (isUwp) {
    genArgs.push("-DCMAKE_SYSTEM_NAME=WindowsStore");
    genArgs.push(`-DCMAKE_SYSTEM_VERSION="${args["windows-sdk-version"]}"`);
    genArgs.push(
      `-DIMPORT_HERMESC=\"${path.join(toolsPath, "ImportHermesc.cmake")}\"`
    );
  } else if (platform === "arm64" || platform === "arm64ec") {
    genArgs.push("-DHERMES_MSVC_ARM64=ON");
    genArgs.push(
      `-DIMPORT_HERMESC=\"${path.join(toolsPath, "ImportHermesc.cmake")}\"`
    );
  }

  runCMakeCommand(`cmake ${genArgs.join(" ")} \"${sourcesPath}\"`, buildParams);
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
    console.log("Skip testing for UWP and ARM64/ARM64EC builds");
    return;
  }

  if (!fs.existsSync(buildParams.buildPath)) {
    cmakeBuild(buildParams);
  }

  runCMakeCommand("ctest --output-on-failure", buildParams);
}

function cmakeBuildHermesCompiler(buildParams) {
  const { toolsPath } = buildParams;

  // Only build hermesc explicitly for UWP and ARM64/ARM64EC.
  if (!isCrossPlatformBuild(buildParams)) {
    return;
  }

  const hermesCompilerPath = path.join(toolsPath, "bin", "hermesc.exe");
  if (!fs.existsSync(hermesCompilerPath)) {
    cmakeBuild({
      ...buildParams,
      isUwp: false,
      platform: "x64",
      configuration: "release",
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
  } finally {
    process.chdir(originalCwd);
    console.log(`Changed CWD back to: ${originalCwd}`);
  }
}

function isCrossPlatformBuild({ isUwp, platform }) {
  // Return true if we either build for UWP and ARM64/ARM64EC.
  // In these cases we must build x64 tools and cannot run unit tests.
  return isUwp || platform.startsWith("arm64");
}

function copyBuiltFilesToPkgStaging(buildParams) {
  const { buildPath } = buildParams;
  const { dllStagingPath, toolsStagingPath } = ensureStagingPaths(buildParams);

  const dllSourcePath = path.join(buildPath, "API", "hermes_shared");
  copyFile("hermes.dll", dllSourcePath, dllStagingPath);
  copyFile("hermes.lib", dllSourcePath, dllStagingPath);
  copyFile("hermes.pdb", dllSourcePath, dllStagingPath);

  if (!isCrossPlatformBuild(buildParams)) {
    const toolsSourcePath = path.join(buildPath, "bin");
    copyFile("hermes.exe", toolsSourcePath, toolsStagingPath);
    copyFile("hermesc.exe", toolsSourcePath, toolsStagingPath);
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

function copyFile(fileName, sourcePath, targetPath) {
  ensureDir(targetPath);
  fs.copyFileSync(
    path.join(sourcePath, fileName),
    path.join(targetPath, fileName)
  );
}

function createFakeBinFile(targetPath, fileName) {
  fs.copyFileSync(
    path.join(process.env.SystemRoot, "system32", "kernel32.dll"),
    path.join(targetPath, fileName)
  );
}

function ensureStagingPaths(buildParams) {
  const { isUwp, platform, configuration } = buildParams;

  const dllStagingPath = path.join(
    buildParams.pkgStagingPath,
    "lib",
    "native",
    getAppPlatformName(isUwp),
    configuration,
    platform
  );
  ensureDir(dllStagingPath);

  const toolsStagingPath = path.join(
    buildParams.pkgStagingPath,
    "tools",
    "native",
    configuration,
    platform
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

  // Copy node-api headers
  const hermesSharedApiPath = path.join(hermesApiPath, "hermes_shared");
  const hermesNodeApiPath = path.join(hermesSharedApiPath, "node-api");
  const stagingNodeApiPath = path.join(stagingIncludePath, "node-api");
  const stagingHermesApiPath = path.join(stagingIncludePath, "hermes");
  ensureDir(stagingNodeApiPath);
  copyFile("js_native_api.h", hermesNodeApiPath, stagingNodeApiPath);
  copyFile("js_native_api_types.h", hermesNodeApiPath, stagingNodeApiPath);
  copyFile("js_runtime_api.h", hermesNodeApiPath, stagingNodeApiPath);
  ensureDir(stagingHermesApiPath);
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
    stagingBuildNativePath
  );
  copyFile(
    "Microsoft.JavaScript.Hermes.targets",
    nugetSourcePath,
    stagingBuildNativePath
  );
  copyFile(
    "Microsoft.JavaScript.Hermes.nuspec",
    nugetSourcePath,
    pkgStagingPath
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
    "Microsoft.JavaScript.Hermes.nuspec"
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
    "vswhere.exe"
  );
  if (!fs.existsSync(vsWhere)) {
    throw new Error("Could not find vswhere.exe");
  }

  const versionJson = JSON.parse(
    execSync(`"${vsWhere}" -format json -version 17`).toString()
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
    "vcvarsall.bat"
  );
  if (!fs.existsSync(vcVarsAllBat)) {
    throw new Error(
      `Could not find vcvarsall.bat at expected Visual Studio installation path: ${vcVarsAllBat}`
    );
  }

  return vcVarsAllBat;
}

function getVCVarsAllBatArgs(buildParams) {
  const { platform, isUwp } = buildParams;
  let vcArgs = "";
  switch (platform) {
    case "x64":
      vcArgs += "x64";
      break;
    case "x86":
      vcArgs += "x64_x86";
      break;
    case "arm64":
    case "arm64ec":
      vcArgs += "x64_arm64";
      break;
    default:
      vcArgs += "x64";
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
