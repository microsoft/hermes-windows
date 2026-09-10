// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import yaml from "js-yaml";

const APPROVED_REGISTRY =
  "https://pkgs.dev.azure.com/ms/react-native/_packaging/react-native-public/npm/registry/";
const YARN_VERSION = "4.13.0";
const YARN_PACKAGE_MANAGER = `yarn@${YARN_VERSION}`;
const YARN_PATH = `.yarn/releases/yarn-${YARN_VERSION}.cjs`;
const DEPENDENCY_FIELDS = [
  "dependencies",
  "devDependencies",
  "optionalDependencies",
  "peerDependencies",
  "resolutions",
  "overrides",
] as const;
const INSTALL_DEPENDENCY_FIELDS = [
  "dependencies",
  "devDependencies",
  "optionalDependencies",
  "resolutions",
  "overrides",
] as const;
const BLOCKED_DOMAINS = [
  "registry.npmjs.org",
  "npmjs.com",
  "registry.yarnpkg.com",
  "yarnpkg.com",
] as const;

type JsonObject = Record<string, unknown>;

interface Project {
  directory: string;
  kind: "npm" | "yarn";
  lockfile: string;
}

interface ProjectResult {
  directory: string;
  kind: "npm" | "yarn";
  errorCount: number;
}

export interface LintResult {
  errors: string[];
  projects: ProjectResult[];
}

export interface LintOptions {
  emit?: boolean;
}

export function isLegacyYarnLock(content: string): boolean {
  return /^# yarn lockfile v1$/m.test(content);
}

export function getNodeVersionError(version: string): string | undefined {
  const major = Number.parseInt(version.split(".")[0] ?? "", 10);
  if (!Number.isInteger(major) || major < 24) {
    return `Node.js 24 or newer is required; found ${version}.`;
  }
  return undefined;
}

export function lintRepository(
  repositoryRoot: string,
  options: LintOptions = {},
): LintResult {
  const root = path.resolve(repositoryRoot);
  const errors: string[] = [];
  const projectResults: ProjectResult[] = [];
  const emit = options.emit ?? true;
  const nodeVersionError = getNodeVersionError(process.versions.node);
  if (nodeVersionError) {
    errors.push(nodeVersionError);
  }

  let sourceFiles: Set<string>;
  try {
    sourceFiles = getSourceFiles(root);
  } catch (error) {
    errors.push(
      `git: unable to inventory repository files: ${getErrorMessage(error)}`,
    );
    return finish(errors, projectResults, emit);
  }

  const dependabotDirectories = getDependabotDirectories(
    root,
    sourceFiles,
    errors,
  );
  validateBlockedDomains(root, sourceFiles, errors);
  validatePackageManifests(root, sourceFiles, dependabotDirectories, errors);
  const projects = getProjects(
    root,
    sourceFiles,
    dependabotDirectories,
    errors,
  );

  for (const project of projects) {
    const before = errors.length;
    if (project.kind === "npm") {
      validateNpmProject(root, sourceFiles, project, errors);
    } else {
      validateYarnProject(root, sourceFiles, project, errors);
    }
    projectResults.push({
      directory: displayDirectory(project.directory),
      kind: project.kind,
      errorCount: errors.length - before,
    });
  }

  return finish(errors, projectResults, emit);
}

function finish(
  errors: string[],
  projects: ProjectResult[],
  emit: boolean,
): LintResult {
  if (emit) {
    for (const project of projects) {
      const status = project.errorCount === 0 ? "PASS" : "FAIL";
      console.log(`${status} ${project.kind} ${project.directory}`);
    }
    for (const error of errors) {
      console.error(`ERROR ${error}`);
    }
    const npmCount = projects.filter(({ kind }) => kind === "npm").length;
    const yarnCount = projects.length - npmCount;
    console.log(
      `Checked ${projects.length} package projects (${npmCount} npm, ${yarnCount} Yarn); ${errors.length} violation(s).`,
    );
  }
  return { errors, projects };
}

function getSourceFiles(root: string): Set<string> {
  const output = execFileSync(
    "git",
    [
      "-C",
      root,
      "ls-files",
      "--cached",
      "--others",
      "--exclude-standard",
      "-z",
    ],
    { encoding: "utf8", maxBuffer: 16 * 1024 * 1024 },
  );
  return new Set(
    output
      .split("\0")
      .filter(Boolean)
      .map((file) => normalizePath(file))
      .filter((file) => fs.existsSync(path.join(root, file))),
  );
}

function validateBlockedDomains(
  root: string,
  sourceFiles: Set<string>,
  errors: string[],
): void {
  const sourceFilesToCheck = [...sourceFiles].filter((file) =>
    /(^|\/)(?:package-lock\.json|yarn\.lock|\.npmrc|\.yarnrc(?:\.yml)?)$/.test(
      file,
    ),
  );
  for (const file of sourceFilesToCheck) {
    const content = readText(root, file, errors);
    if (content === undefined) {
      continue;
    }
    const lowerContent = content.toLowerCase();
    for (const domain of BLOCKED_DOMAINS) {
      const index = lowerContent.indexOf(domain);
      if (index !== -1) {
        errors.push(
          formatError(
            file,
            lineAt(content, index),
            "package source",
            domain,
            `sources under ${APPROVED_REGISTRY}`,
          ),
        );
      }
    }
  }
}

function validatePackageManifests(
  root: string,
  sourceFiles: Set<string>,
  dependabotDirectories: string[],
  errors: string[],
): void {
  for (const file of [...sourceFiles].filter((entry) =>
    /(^|\/)package\.json$/.test(entry),
  )) {
    const manifest = readJson(root, file, errors);
    if (!manifest) {
      continue;
    }
    for (const field of DEPENDENCY_FIELDS) {
      const value = manifest[field];
      if (value !== undefined) {
        validateDependencyValue(root, file, field, value, errors);
      }
    }
    const directory = normalizePath(path.posix.dirname(file));
    const projectDirectory = directory === "." ? "" : directory;
    if (
      isInstallCapableManifest(manifest) &&
      !dependabotDirectories.includes(projectDirectory) &&
      !isOwnedWorkspace(root, projectDirectory, dependabotDirectories, errors)
    ) {
      errors.push(
        `${file}: install-capable package manifest is not owned by a Dependabot npm directory or one of its declared workspaces.`,
      );
    }
  }
}

function isInstallCapableManifest(manifest: JsonObject): boolean {
  if (manifest.packageManager !== undefined) {
    return true;
  }
  return INSTALL_DEPENDENCY_FIELDS.some((field) => {
    const value = manifest[field];
    return isObject(value) && Object.keys(value).length > 0;
  });
}

function validateDependencyValue(
  root: string,
  file: string,
  field: string,
  value: unknown,
  errors: string[],
): void {
  if (typeof value === "string") {
    const sourceError = getDependencySourceError(root, file, value);
    if (sourceError) {
      const content = fs.readFileSync(path.join(root, file), "utf8");
      errors.push(
        formatError(file, findLine(content, value), field, value, sourceError),
      );
    }
    return;
  }
  if (!isObject(value)) {
    return;
  }
  for (const [key, child] of Object.entries(value)) {
    validateDependencyValue(root, file, `${field}.${key}`, child, errors);
  }
}

function getDependencySourceError(
  root: string,
  file: string,
  value: string,
): string | undefined {
  const source = value.trim();
  const decodedSource = decodeReference(source);
  if (decodedSource !== source) {
    return getDependencySourceError(root, file, decodedSource);
  }
  const protocol = /^([a-z][a-z0-9+.-]*):(.*)$/i.exec(source);
  if (!protocol) {
    if (/^(?:\.\.?[\\/]|[A-Za-z]:[\\/]|[\\/]{2})/.test(source)) {
      return getLocalPathError(root, file, source);
    }
    if (hasRemoteReference(source)) {
      return "a semver, npm alias, workspace:, or repository-local path";
    }
    return undefined;
  }
  const scheme = protocol[1].toLowerCase();
  const reference = protocol[2];
  if (scheme === "npm" || scheme === "workspace") {
    return hasRemoteReference(reference)
      ? "a semver, npm alias, workspace:, or repository-local path"
      : undefined;
  }
  if (scheme === "file" || scheme === "link" || scheme === "portal") {
    return getLocalPathError(root, file, reference);
  }
  if (scheme === "patch" && !hasRemoteReference(reference)) {
    const [locator, patchPath] = reference.split("#", 2);
    const locatorError = getDependencySourceError(root, file, locator);
    if (locatorError) {
      return locatorError;
    }
    return patchPath
      ? getDependencySourceError(
          root,
          file,
          patchPath.replace(/^optional!/, ""),
        )
      : undefined;
  }
  return "a semver, npm alias, workspace:, or repository-local path";
}

function getLocalPathError(
  root: string,
  file: string,
  reference: string,
): string | undefined {
  if (!reference || /^[\\/]{2}/.test(reference)) {
    return "a local path contained within the repository";
  }
  const manifestDirectory = path.dirname(path.join(root, file));
  const target = path.resolve(manifestDirectory, reference);
  let canonicalRoot: string;
  let canonicalTarget: string;
  try {
    canonicalRoot = fs.realpathSync.native(root);
    canonicalTarget = fs.realpathSync.native(target);
  } catch {
    return "an existing local path contained within the repository";
  }
  const relative = path.relative(canonicalRoot, canonicalTarget);
  if (
    relative === ".." ||
    relative.startsWith(`..${path.sep}`) ||
    path.isAbsolute(relative)
  ) {
    return "a local path contained within the repository";
  }
  return undefined;
}

function decodeReference(value: string): string {
  try {
    return decodeURIComponent(value);
  } catch {
    return value;
  }
}

function hasRemoteReference(value: string): boolean {
  return (
    /(?:^|@)(?:https?:\/\/|git(?:\+[^:]+)?:|ssh:|github:|gitlab:|bitbucket:|git@)/i.test(
      value,
    ) ||
    /^(?:[A-Za-z0-9_.-]+\/){1,2}[A-Za-z0-9_.-]+(?:#.*)?$/.test(value) ||
    /@(?:[A-Za-z0-9_.-]+\/){1,2}[A-Za-z0-9_.-]+(?:#.*)?$/.test(value)
  );
}

function getDependabotDirectories(
  root: string,
  sourceFiles: Set<string>,
  errors: string[],
): string[] {
  const file = ".github/dependabot.yml";
  if (!sourceFiles.has(file)) {
    errors.push(`${file}: required npm update configuration is missing.`);
    return [];
  }
  const content = readText(root, file, errors);
  if (content === undefined) {
    return [];
  }
  let document: unknown;
  try {
    document = yaml.load(content);
  } catch (error) {
    errors.push(`${file}: invalid YAML: ${getErrorMessage(error)}`);
    return [];
  }
  if (!isObject(document) || !Array.isArray(document.updates)) {
    errors.push(`${file}: updates must be an array.`);
    return [];
  }

  const directories: string[] = [];
  for (const [updateIndex, update] of document.updates.entries()) {
    if (!isObject(update) || update["package-ecosystem"] !== "npm") {
      continue;
    }
    const values: unknown[] = [];
    if (update.directory !== undefined) {
      values.push(update.directory);
    }
    if (Array.isArray(update.directories)) {
      values.push(...update.directories);
    }
    if (values.length === 0) {
      errors.push(
        `${file}: updates[${updateIndex}] must define directory or directories.`,
      );
    }
    for (const value of values) {
      if (typeof value !== "string") {
        errors.push(
          `${file}: updates[${updateIndex}] contains a non-string npm directory.`,
        );
        continue;
      }
      const directory = normalizeDirectory(value);
      if (directory === undefined) {
        errors.push(
          formatError(
            file,
            findLine(content, value),
            "npm directory",
            value,
            "an absolute repository directory without '..'",
          ),
        );
        continue;
      }
      if (directories.includes(directory)) {
        errors.push(
          formatError(
            file,
            findLine(content, value),
            "npm directory",
            value,
            "a unique directory",
          ),
        );
        continue;
      }
      directories.push(directory);
    }
  }

  for (const directory of directories) {
    const packageJson = joinRelative(directory, "package.json");
    if (!fs.existsSync(path.join(root, directory))) {
      errors.push(
        `${file}: npm directory ${displayDirectory(directory)} does not exist.`,
      );
    } else if (!sourceFiles.has(packageJson)) {
      errors.push(
        `${file}: npm directory ${displayDirectory(directory)} must contain a direct package.json.`,
      );
    }
  }
  return directories;
}

function getProjects(
  root: string,
  sourceFiles: Set<string>,
  dependabotDirectories: string[],
  errors: string[],
): Project[] {
  const locksByDirectory = new Map<string, string[]>();
  for (const file of sourceFiles) {
    if (!/(^|\/)(?:package-lock\.json|yarn\.lock)$/.test(file)) {
      continue;
    }
    const directory = normalizePath(path.posix.dirname(file));
    const key = directory === "." ? "" : directory;
    const locks = locksByDirectory.get(key) ?? [];
    locks.push(file);
    locksByDirectory.set(key, locks);
  }

  for (const directory of dependabotDirectories) {
    const locks = locksByDirectory.get(directory) ?? [];
    if (locks.length !== 1) {
      errors.push(
        `.github/dependabot.yml: npm directory ${displayDirectory(directory)} must have exactly one authoritative package-lock.json or yarn.lock; found ${locks.length}.`,
      );
    }
  }

  const projects: Project[] = [];
  for (const [directory, locks] of [...locksByDirectory].sort(
    ([left], [right]) => left.localeCompare(right),
  )) {
    if (locks.length !== 1) {
      errors.push(
        `${displayDirectory(directory)}: multiple authoritative lockfiles found: ${locks.join(", ")}.`,
      );
      continue;
    }
    const packageJson = joinRelative(directory, "package.json");
    if (!sourceFiles.has(packageJson)) {
      errors.push(`${locks[0]}: lockfile has no direct package.json owner.`);
      continue;
    }
    if (
      !dependabotDirectories.includes(directory) &&
      !isOwnedWorkspace(root, directory, dependabotDirectories, errors)
    ) {
      errors.push(
        `${locks[0]}: lockfile is not owned by a Dependabot npm directory or one of its declared workspaces.`,
      );
    }
    projects.push({
      directory,
      kind: locks[0].endsWith("package-lock.json") ? "npm" : "yarn",
      lockfile: locks[0],
    });
  }
  return projects;
}

function isOwnedWorkspace(
  root: string,
  directory: string,
  dependabotDirectories: string[],
  errors: string[],
): boolean {
  const owners = dependabotDirectories
    .filter((owner) => owner === "" || directory.startsWith(`${owner}/`))
    .sort((left, right) => right.length - left.length);
  for (const owner of owners) {
    const relative =
      owner === "" ? directory : directory.slice(owner.length + 1);
    const manifest = readJson(
      root,
      joinRelative(owner, "package.json"),
      errors,
    );
    if (!manifest) {
      continue;
    }
    const workspaces = Array.isArray(manifest.workspaces)
      ? manifest.workspaces
      : isObject(manifest.workspaces) &&
          Array.isArray(manifest.workspaces.packages)
        ? manifest.workspaces.packages
        : [];
    if (
      workspaces.some(
        (workspace) =>
          typeof workspace === "string" &&
          matchesWorkspace(normalizePath(workspace), relative),
      )
    ) {
      return true;
    }
  }
  return false;
}

function matchesWorkspace(pattern: string, relative: string): boolean {
  const patternParts = pattern.split("/");
  const relativeParts = relative.split("/");
  if (patternParts.length !== relativeParts.length) {
    return false;
  }
  return patternParts.every(
    (part, index) => part === "*" || part === relativeParts[index],
  );
}

function validateNpmProject(
  root: string,
  sourceFiles: Set<string>,
  project: Project,
  errors: string[],
): void {
  const npmrc = joinRelative(project.directory, ".npmrc");
  if (!sourceFiles.has(npmrc)) {
    errors.push(`${npmrc}: committed npm registry configuration is missing.`);
  } else {
    const content = readText(root, npmrc, errors);
    if (content !== undefined) {
      const config = parseNpmrc(content);
      requireConfigValue(
        npmrc,
        content,
        config,
        "registry",
        APPROVED_REGISTRY,
        errors,
      );
      requireConfigValue(
        npmrc,
        content,
        config,
        "omit-lockfile-registry-resolved",
        "true",
        errors,
      );
      for (const [key, value] of config) {
        if (key !== "registry" && key.endsWith(":registry")) {
          if (value !== APPROVED_REGISTRY) {
            errors.push(
              formatError(
                npmrc,
                findLine(content, value),
                key,
                value,
                APPROVED_REGISTRY,
              ),
            );
          }
        }
        if (isCredentialKey(key)) {
          errors.push(
            `${npmrc}:${findLine(content, key) ?? 1}: committed npm credentials are forbidden (${key}).`,
          );
        }
      }
    }
  }

  const lock = readJson(root, project.lockfile, errors);
  if (!lock) {
    return;
  }
  if (typeof lock.lockfileVersion !== "number" || lock.lockfileVersion < 2) {
    errors.push(
      `${project.lockfile}: lockfileVersion must be 2 or newer; found ${String(lock.lockfileVersion)}.`,
    );
  }
  visitObject(lock, "", (field, value) => {
    if (field.split(".").at(-1) !== "resolved" || typeof value !== "string") {
      return;
    }
    if (/^https?:\/\//i.test(value)) {
      const content = fs.readFileSync(
        path.join(root, project.lockfile),
        "utf8",
      );
      const required = value.startsWith(APPROVED_REGISTRY)
        ? "registry dependencies with no resolved field"
        : `a source under ${APPROVED_REGISTRY}`;
      errors.push(
        formatError(
          project.lockfile,
          findLine(content, value),
          field,
          value,
          required,
        ),
      );
    } else {
      const sourceError = getDependencySourceError(
        root,
        joinRelative(project.directory, "package.json"),
        value,
      );
      if (sourceError) {
        errors.push(
          formatError(
            project.lockfile,
            findLine(
              fs.readFileSync(path.join(root, project.lockfile), "utf8"),
              value,
            ),
            field,
            value,
            sourceError,
          ),
        );
      }
    }
  });
}

function validateYarnProject(
  root: string,
  sourceFiles: Set<string>,
  project: Project,
  errors: string[],
): void {
  const packageJsonFile = joinRelative(project.directory, "package.json");
  const manifest = readJson(root, packageJsonFile, errors);
  if (manifest && manifest.packageManager !== YARN_PACKAGE_MANAGER) {
    const content = fs.readFileSync(path.join(root, packageJsonFile), "utf8");
    errors.push(
      formatError(
        packageJsonFile,
        typeof manifest.packageManager === "string"
          ? findLine(content, manifest.packageManager)
          : undefined,
        "packageManager",
        String(manifest.packageManager),
        YARN_PACKAGE_MANAGER,
      ),
    );
  }

  const configFile = findYarnConfig(root, sourceFiles, project.directory);
  if (!configFile) {
    errors.push(
      `${displayDirectory(project.directory)}: no committed .yarnrc.yml applies to this Yarn project.`,
    );
  } else {
    validateYarnConfig(root, sourceFiles, configFile, errors);
  }

  const content = readText(root, project.lockfile, errors);
  if (content === undefined) {
    return;
  }
  if (isLegacyYarnLock(content)) {
    errors.push(
      `${project.lockfile}: legacy Yarn 1 lockfile format is forbidden.`,
    );
    return;
  }
  let lock: unknown;
  try {
    lock = yaml.load(content);
  } catch (error) {
    errors.push(
      `${project.lockfile}: invalid Yarn lockfile YAML: ${getErrorMessage(error)}`,
    );
    return;
  }
  if (
    !isObject(lock) ||
    !isObject(lock.__metadata) ||
    !Number.isInteger(lock.__metadata.version) ||
    Number(lock.__metadata.version) < 8
  ) {
    errors.push(
      `${project.lockfile}: Yarn 4 lockfile metadata version 8 or newer is required.`,
    );
    return;
  }
  visitObject(lock, "", (field, value) => {
    if (typeof value !== "string") {
      return;
    }
    for (const source of value.match(/https?:\/\/[^\s"'<>]+/gi) ?? []) {
      if (!source.startsWith(APPROVED_REGISTRY)) {
        errors.push(
          formatError(
            project.lockfile,
            findLine(content, source),
            field,
            source,
            `a source under ${APPROVED_REGISTRY}`,
          ),
        );
      }
    }
    if (field.split(".").at(-1) === "resolution") {
      const reference = getLocatorReference(value);
      const sourceError = getDependencySourceError(
        root,
        joinRelative(project.directory, "package.json"),
        reference,
      );
      if (!sourceError) {
        return;
      }
      errors.push(
        formatError(
          project.lockfile,
          findLine(content, value),
          field,
          value,
          sourceError,
        ),
      );
    }
  });
}

function getLocatorReference(locator: string): string {
  const packageSeparator = locator.startsWith("@")
    ? locator.indexOf("@", locator.indexOf("/") + 1)
    : locator.indexOf("@");
  return packageSeparator === -1
    ? locator
    : locator.slice(packageSeparator + 1);
}

function findYarnConfig(
  root: string,
  sourceFiles: Set<string>,
  projectDirectory: string,
): string | undefined {
  let directory = projectDirectory;
  while (true) {
    const candidate = joinRelative(directory, ".yarnrc.yml");
    if (
      sourceFiles.has(candidate) &&
      fs.existsSync(path.join(root, candidate))
    ) {
      return candidate;
    }
    if (directory === "") {
      return undefined;
    }
    const parent = normalizePath(path.posix.dirname(directory));
    directory = parent === "." ? "" : parent;
  }
}

function validateYarnConfig(
  root: string,
  sourceFiles: Set<string>,
  file: string,
  errors: string[],
): void {
  const content = readText(root, file, errors);
  if (content === undefined) {
    return;
  }
  let config: unknown;
  try {
    config = yaml.load(content);
  } catch (error) {
    errors.push(`${file}: invalid YAML: ${getErrorMessage(error)}`);
    return;
  }
  if (!isObject(config)) {
    errors.push(`${file}: Yarn configuration must be a mapping.`);
    return;
  }
  requireObjectValue(
    file,
    content,
    config,
    "npmRegistryServer",
    APPROVED_REGISTRY,
    errors,
  );
  requireObjectValue(
    file,
    content,
    config,
    "nodeLinker",
    "node-modules",
    errors,
  );
  requireObjectValue(file, content, config, "yarnPath", YARN_PATH, errors);
  visitObject(config, "", (field, value) => {
    const key = field.split(".").at(-1)?.toLowerCase() ?? "";
    if (key === "npmregistryserver" && value !== APPROVED_REGISTRY) {
      errors.push(
        formatError(
          file,
          typeof value === "string" ? findLine(content, value) : undefined,
          field,
          String(value),
          APPROVED_REGISTRY,
        ),
      );
    }
    if (key === "npmauthident" || key === "npmauthtoken") {
      errors.push(
        `${file}:${typeof value === "string" ? (findLine(content, value) ?? 1) : 1}: committed Yarn credentials are forbidden (${field}).`,
      );
    }
  });
  if (config.yarnPath === YARN_PATH) {
    const configDirectory = normalizePath(path.posix.dirname(file));
    const release = joinRelative(
      configDirectory === "." ? "" : configDirectory,
      YARN_PATH,
    );
    if (!sourceFiles.has(release) || !fs.existsSync(path.join(root, release))) {
      errors.push(
        `${file}: yarnPath target ${release} must be present in Git.`,
      );
    }
  }
}

function parseNpmrc(content: string): Map<string, string> {
  const config = new Map<string, string>();
  for (const line of content.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#") || trimmed.startsWith(";")) {
      continue;
    }
    const equals = trimmed.indexOf("=");
    if (equals === -1) {
      continue;
    }
    config.set(
      trimmed.slice(0, equals).trim().toLowerCase(),
      trimmed.slice(equals + 1).trim(),
    );
  }
  return config;
}

function isCredentialKey(key: string): boolean {
  return /(^|:)(?:_auth|_authtoken|username|_password|password)$/i.test(key);
}

function requireConfigValue(
  file: string,
  content: string,
  config: Map<string, string>,
  key: string,
  required: string,
  errors: string[],
): void {
  const observed = config.get(key);
  if (observed !== required) {
    errors.push(
      formatError(
        file,
        observed === undefined ? undefined : findLine(content, observed),
        key,
        String(observed),
        required,
      ),
    );
  }
}

function requireObjectValue(
  file: string,
  content: string,
  object: JsonObject,
  key: string,
  required: string,
  errors: string[],
): void {
  const observed = object[key];
  if (observed !== required) {
    errors.push(
      formatError(
        file,
        typeof observed === "string" ? findLine(content, observed) : undefined,
        key,
        String(observed),
        required,
      ),
    );
  }
}

function readJson(
  root: string,
  file: string,
  errors: string[],
): JsonObject | undefined {
  const content = readText(root, file, errors);
  if (content === undefined) {
    return undefined;
  }
  try {
    const value: unknown = JSON.parse(content);
    if (!isObject(value)) {
      errors.push(`${file}: JSON root must be an object.`);
      return undefined;
    }
    return value;
  } catch (error) {
    errors.push(`${file}: invalid JSON: ${getErrorMessage(error)}`);
    return undefined;
  }
}

function readText(
  root: string,
  file: string,
  errors: string[],
): string | undefined {
  try {
    return fs.readFileSync(path.join(root, file), "utf8");
  } catch (error) {
    errors.push(`${file}: unable to read file: ${getErrorMessage(error)}`);
    return undefined;
  }
}

function visitObject(
  value: unknown,
  field: string,
  visitor: (field: string, value: unknown) => void,
): void {
  visitor(field, value);
  if (Array.isArray(value)) {
    value.forEach((child, index) =>
      visitObject(child, `${field}[${index}]`, visitor),
    );
  } else if (isObject(value)) {
    for (const [key, child] of Object.entries(value)) {
      visitObject(child, field ? `${field}.${key}` : key, visitor);
    }
  }
}

function isObject(value: unknown): value is JsonObject {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function normalizeDirectory(value: string): string | undefined {
  if (!value.startsWith("/") || value.includes("..")) {
    return undefined;
  }
  return normalizePath(value).replace(/^\/+|\/+$/g, "");
}

function normalizePath(value: string): string {
  return value.replaceAll("\\", "/");
}

function joinRelative(directory: string, file: string): string {
  return directory ? `${directory}/${file}` : file;
}

function displayDirectory(directory: string): string {
  return directory ? `/${directory}` : "/";
}

function findLine(content: string, value: string): number | undefined {
  const index = content.indexOf(value);
  return index === -1 ? undefined : lineAt(content, index);
}

function lineAt(content: string, index: number): number {
  return content.slice(0, index).split("\n").length;
}

function formatError(
  file: string,
  line: number | undefined,
  field: string,
  observed: string,
  required: string,
): string {
  const location = line === undefined ? file : `${file}:${line}`;
  return `${location}: ${field} is ${JSON.stringify(observed)}; required ${required}.`;
}

function getErrorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function findRepositoryRoot(): string {
  return execFileSync("git", ["rev-parse", "--show-toplevel"], {
    encoding: "utf8",
  }).trim();
}

function runCli(): void {
  const versionError = getNodeVersionError(process.versions.node);
  if (versionError) {
    console.error(`ERROR ${versionError}`);
    process.exitCode = 1;
    return;
  }
  try {
    const result = lintRepository(findRepositoryRoot());
    if (result.errors.length > 0) {
      process.exitCode = 1;
    }
  } catch (error) {
    console.error(`ERROR ${getErrorMessage(error)}`);
    process.exitCode = 1;
  }
}

if (
  process.argv[1] &&
  path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  runCli();
}
