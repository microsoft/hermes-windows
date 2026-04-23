// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

//
// Script to build Hermes Apple frameworks (macOS, iOS, tvOS, visionOS).
// See the options and printHelp() below for the usage details.
//
// This script is intended to be run in the Azure DevOps pipeline
// or locally with Node.js on a macOS host.
// It delegates to the upstream shell scripts (build-apple-framework.sh,
// build-ios-framework.sh) which handle CMake configuration, compilation,
// and xcframework assembly.
//
// Usage:
//   node build-apple.mts                  # Full local build (hermesc + all slices + xcframework)
//   node build-apple.mts --slice macosx   # Build hermesc + macosx slice + xcframework
//   node build-apple.mts --no-build --no-build-xcframework   # CI: hermesc only
//

import { execSync } from "node:child_process";
import { existsSync, mkdtempSync, mkdirSync, cpSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { parseArgs } from "node:util";
import { fileURLToPath } from "node:url";

const __filename: string = fileURLToPath(import.meta.url);
const __dirname: string = path.dirname(__filename);

// The root of the local Hermes repository.
const sourcesPath: string = path.resolve(__dirname, path.join("..", ".."));

const APPLE_SLICES: string[] = [
  "macosx",
  "iphoneos",
  "iphonesimulator",
  "catalyst",
  "appletvos",
  "appletvsimulator",
  "xros",
  "xrsimulator",
];

const options = {
  help: { type: "boolean" as const, default: false },

  // Action flags — all default true so a bare invocation builds everything.
  // CI uses --no-build-hermesc, --no-build, --no-build-xcframework to run
  // individual phases, just like build.js uses --no-build --test etc.
  "build-hermesc": { type: "boolean" as const, default: true },
  build: { type: "boolean" as const, default: true },
  "build-xcframework": { type: "boolean" as const, default: true },

  // Scoping — when empty, all slices are built.
  slice: {
    type: "string" as const,
    multiple: true as const,
    default: [] as string[],
  },

  // Build configuration.
  configuration: {
    type: "string" as const,
    default: "release",
  },

  // Deployment targets.
  "ios-deployment-target": { type: "string" as const, default: "15.1" },
  "macos-deployment-target": { type: "string" as const, default: "10.15" },
  "xros-deployment-target": { type: "string" as const, default: "1.0" },

  // CI helpers.
  "create-dsym-archive": { type: "boolean" as const, default: false },
};

const validSets: Record<string, string[]> = {
  slice: APPLE_SLICES,
  configuration: ["debug", "release"],
};

const { values: args } = parseArgs({ options, allowNegative: true });

const scriptRelativePath: string = path.relative(process.cwd(), __filename);

if (args.help) {
  printHelp();
  process.exit(0);
}

function printHelp(): void {
  console.log(`Usage: node ${scriptRelativePath} [options]

Build Hermes Apple frameworks. Requires macOS with Xcode installed.

All boolean flags support negation (e.g., --no-build, --no-build-hermesc).
With no flags, a full build is performed: hermesc -> all slices -> xcframework.

Options:
  --help                     Show this help message and exit
  --build-hermesc            Build the host hermesc compiler (default: true)
  --build                    Build platform slice(s) (default: true)
  --build-xcframework        Assemble slices into universal xcframework (default: true)
  --slice                    Specific slice(s) to build (default: all)
                             [valid: ${APPLE_SLICES.join(", ")}]
  --configuration            Build configuration (default: release)
                             [valid: debug, release]
  --ios-deployment-target    iOS/tvOS deployment target (default: 15.1)
  --macos-deployment-target  macOS deployment target (default: 10.15)
  --xros-deployment-target   visionOS deployment target (default: 1.0)
  --create-dsym-archive      Create dSYM tar.gz archive (default: false)

Examples (local full build):
  node ${scriptRelativePath}                                    # Build everything
  node ${scriptRelativePath} --configuration debug              # Full debug build
  node ${scriptRelativePath} --slice macosx --slice iphoneos    # Specific slices only

Examples (CI — selective phases):
  node ${scriptRelativePath} --no-build --no-build-xcframework                          # hermesc only
  node ${scriptRelativePath} --no-build-hermesc --slice iphoneos --no-build-xcframework # one slice
  node ${scriptRelativePath} --no-build-hermesc --no-build --create-dsym-archive        # xcframework + dSYM
`);
}

// Normalise string args to lower case.
for (const [key, value] of Object.entries(args)) {
  if (typeof value === "string") {
    (args as Record<string, unknown>)[key] = (value as string).toLowerCase();
  }
}

// Validate args against valid sets.
for (const [key, allowed] of Object.entries(validSets)) {
  const raw = (args as Record<string, unknown>)[key];
  const values: string[] = Array.isArray(raw) ? raw : [raw as string];
  for (const item of values) {
    if (item && !allowed.includes(item)) {
      console.error(`Invalid value for --${key}: ${item}`);
      console.error(`Valid values are: ${allowed.join(", ")}`);
      process.exit(1);
    }
  }
}

// ── Main ────────────────────────────────────────────────────────────────────

main();

function main(): void {
  const startTime: number = Date.now();
  const utilsPath: string = path.join(sourcesPath, "utils");

  const env: NodeJS.ProcessEnv = {
    ...process.env,
    IOS_DEPLOYMENT_TARGET: args["ios-deployment-target"] as string,
    XROS_DEPLOYMENT_TARGET: args["xros-deployment-target"] as string,
    MAC_DEPLOYMENT_TARGET: args["macos-deployment-target"] as string,
  };

  const configuration: string = args.configuration as string;
  const slices: string[] =
    (args.slice as string[]).length > 0
      ? (args.slice as string[])
      : APPLE_SLICES;

  console.log();
  console.log("Apple build invoked with parameters:");
  console.log(`  build-hermesc:            ${args["build-hermesc"]}`);
  console.log(`  build (slices):           ${args.build}`);
  console.log(`  build-xcframework:        ${args["build-xcframework"]}`);
  console.log(`  slice:                    ${slices.join(", ")}`);
  console.log(`  configuration:            ${configuration}`);
  console.log(`  ios-deployment-target:    ${env.IOS_DEPLOYMENT_TARGET}`);
  console.log(`  macos-deployment-target:  ${env.MAC_DEPLOYMENT_TARGET}`);
  console.log(`  xros-deployment-target:   ${env.XROS_DEPLOYMENT_TARGET}`);
  console.log(`  create-dsym-archive:      ${args["create-dsym-archive"]}`);
  console.log();

  // Phase 1: Build the host hermesc compiler.
  if (args["build-hermesc"]) {
    buildHermesc(utilsPath, env);
  }

  // Phase 2: Build platform slices.
  if (args.build) {
    for (const slice of slices) {
      buildSlice(utilsPath, slice, configuration, env);
    }
  }

  // Phase 3: Assemble universal xcframework.
  if (args["build-xcframework"]) {
    buildXCFramework(utilsPath, configuration, env);
  }

  // Phase 4 (optional): Create dSYM archive.
  if (args["create-dsym-archive"]) {
    createDsymArchive(configuration);
  }

  const elapsed: number = Date.now() - startTime;
  const totalTime: string = new Date(elapsed).toISOString().substring(11, 19);
  console.log(`\nApple build completed in ${totalTime}`);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

function run(command: string, env: NodeJS.ProcessEnv): void {
  console.log(`> ${command}`);
  execSync(command, {
    stdio: "inherit",
    cwd: sourcesPath,
    env,
    shell: "/bin/bash",
  });
}

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1).toLowerCase();
}

// ── Build phases ────────────────────────────────────────────────────────────

function buildHermesc(utilsPath: string, env: NodeJS.ProcessEnv): void {
  console.log("Building host hermesc compiler...");
  const script: string = path.join(utilsPath, "build-apple-framework.sh");
  run(
    `set -eo pipefail && source "${script}" && build_host_hermesc_if_needed`,
    env,
  );

  // Validate the build produced the expected outputs.
  const hermescBin: string = path.join(
    sourcesPath,
    "build_host_hermesc",
    "bin",
    "hermesc",
  );
  const importCmake: string = path.join(
    sourcesPath,
    "build_host_hermesc",
    "ImportHostCompilers.cmake",
  );
  if (!existsSync(hermescBin)) {
    throw new Error(`hermesc binary not found at ${hermescBin}`);
  }
  if (!existsSync(importCmake)) {
    throw new Error(`ImportHostCompilers.cmake not found at ${importCmake}`);
  }
  console.log("Host hermesc built successfully.\n");
}

function buildSlice(
  utilsPath: string,
  slice: string,
  configuration: string,
  env: NodeJS.ProcessEnv,
): void {
  console.log(`Building Apple ${slice} (${configuration})...`);

  // If hermesc was already built (or downloaded from CI), point to it so the
  // upstream script doesn't try to rebuild it.
  const importCmake: string = path.join(
    sourcesPath,
    "build_host_hermesc",
    "ImportHostCompilers.cmake",
  );
  const buildEnv: NodeJS.ProcessEnv = {
    ...env,
    BUILD_TYPE: capitalize(configuration),
    ...(existsSync(importCmake)
      ? { HERMES_OVERRIDE_HERMESC_PATH: importCmake }
      : {}),
  };

  const script: string = path.join(utilsPath, "build-ios-framework.sh");
  run(`"${script}" "${slice}"`, buildEnv);

  // Validate outputs.
  const buildDir: string = path.join(sourcesPath, `build_${slice}`);
  const frameworkPath: string = path.join(
    buildDir,
    "lib",
    "hermesvm.framework",
  );
  const dsymPath: string = path.join(
    buildDir,
    "lib",
    "hermesvm.framework.dSYM",
  );

  if (!existsSync(frameworkPath)) {
    throw new Error(
      `hermesvm.framework not found at ${frameworkPath} — ${slice} (${configuration}) build failed`,
    );
  }
  if (!existsSync(dsymPath)) {
    throw new Error(
      `hermesvm.framework.dSYM not found at ${dsymPath} — ${slice} (${configuration}) build failed`,
    );
  }

  console.log(`${slice} (${configuration}) built successfully.\n`);
}

function buildXCFramework(
  utilsPath: string,
  configuration: string,
  env: NodeJS.ProcessEnv,
): void {
  console.log(`Assembling universal xcframework (${configuration})...`);
  const buildEnv: NodeJS.ProcessEnv = {
    ...env,
    BUILD_TYPE: capitalize(configuration),
  };

  // Prepare the destroot from pre-built slices.
  const frameworkScript: string = path.join(
    utilsPath,
    "build-apple-framework.sh",
  );
  run(`source "${frameworkScript}" && prepare_dest_root_for_ci`, buildEnv);

  // Create the universal xcframework.
  const iosScript: string = path.join(utilsPath, "build-ios-framework.sh");
  run(`"${iosScript}" build_framework`, buildEnv);

  // Validate output.
  const xcframeworkPath: string = path.join(
    sourcesPath,
    "destroot",
    "Library",
    "Frameworks",
    "universal",
    "hermesvm.xcframework",
  );
  if (!existsSync(xcframeworkPath)) {
    throw new Error(
      `hermesvm.xcframework not found at ${xcframeworkPath} — assembly failed`,
    );
  }

  console.log("Universal xcframework assembled successfully.\n");
}

function createDsymArchive(configuration: string): void {
  console.log(`Creating dSYM archive (${configuration})...`);

  const workingDir: string = mkdtempSync(path.join(tmpdir(), "hermes-dsym-"));
  const dsymRelPath = "lib/hermesvm.framework.dSYM";

  for (const slice of APPLE_SLICES) {
    const src: string = path.join(sourcesPath, `build_${slice}`, dsymRelPath);
    const dest: string = path.join(workingDir, slice);
    mkdirSync(dest, { recursive: true });
    cpSync(src, path.join(dest, "hermesvm.framework.dSYM"), {
      recursive: true,
    });
  }

  const archiveName = `hermes-dSYM-${capitalize(configuration)}.tar.gz`;
  execSync(`tar -C "${workingDir}" -czf "${archiveName}" .`, {
    stdio: "inherit",
    cwd: sourcesPath,
  });

  console.log(`dSYM archive created: ${archiveName}\n`);
}
