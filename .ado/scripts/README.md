# JavaScript Build Scripts

This folder contains the JavaScript build scripts for Hermes Windows, along with tooling to maintain code quality.

## Files

- `build.js` - Main build script for Hermes Windows
- `npm-source-lint.ts` - Validates committed npm and Yarn package sources
- `setVersionNumber.js` - Script to set version numbers

## Code Quality Tools

We use **Prettier** and **ESLint** to maintain consistent code style and catch potential issues.

### Setup

From this directory (`.ado/scripts`), install the dependencies after configuring
the [package feed](#package-feeds):

```bash
npm ci
```

### Available Scripts

- `npm run format` - Format all JavaScript files with Prettier
- `npm run format:check` - Check if files need formatting
- `npm run lint` - Run ESLint to check for code issues
- `npm run lint:fix` - Run ESLint and automatically fix issues
- `npm run lint:format` - Format with Prettier and then run ESLint with auto-fix
- `npm run npm-source-lint` - Validate repository package sources
- `npm run test:npm-source-lint` - Run the source linter's unit tests

### Usage

To format and lint your changes:

```bash
npm run lint:format
```

To check formatting without changing files:

```bash
npm run format:check
```

To run just the linter:

```bash
npm run lint
```

From the repository root, validate package sources with the supported developer command:

```powershell
.\dev npm-source-lint
```

## Package feeds

npm and Yarn restores use the anonymous Microsoft PackageFeedProxy at
`https://packagefeedproxy.microsoft.io/npm/`. Do not add credentials or a direct
public npm registry to repository configuration. NuGet restores and publishing
keep their existing feeds.

The committed `.npmrc` and `.yarnrc.yml` files must not set a registry.
Dependabot reads them but cannot reach PackageFeedProxy, so a committed proxy
registry fails every update with "egress not allowlisted". Restores select the
proxy through their environment instead:

- CI sets `NPM_CONFIG_REGISTRY` and `YARN_NPM_REGISTRY_SERVER` for all jobs
  (`.ado/npm-variables.yml`).
- `.\dev` sets the same variables unless they are already defined.
- The Node-API test build passes the `HERMES_NPM_REGISTRY` CMake variable to
  `npm ci`. It defaults to the proxy; to use your npm configuration instead,
  set it to an empty value in the build tree's `CMakeCache.txt`.

For other npm and Yarn commands, configure the proxy once per user, from the
repository root:

```powershell
npm config set registry https://packagefeedproxy.microsoft.io/npm/ --location=user
node .yarn/releases/yarn-4.13.0.cjs config set --home npmRegistryServer https://packagefeedproxy.microsoft.io/npm/
```

Setting the two environment variables above works as well.

Dependabot resolves updates from the public npm registry. Do not add
`npm-registry` declarations to `dependabot.yml`: GitHub requires credentials for
those entries.

npm and Yarn lockfiles must remain free of package download URLs, so the same
lockfile restores from both the public registry and the proxy.
npm's `omit-lockfile-registry-resolved=true` prevents registry URLs. Yarn can
record an encoded `__archiveUrl` when resolving through the proxy; remove that
binding before committing, without changing versions or checksums, and rerun
`yarn install --immutable` and `.\dev npm-source-lint`. The lint rejects these
bindings, committed registries, and committed credentials.

Keep Yarn's `npmMinimalAgeGate: "8d"` and Dependabot's
`cooldown.default-days: 8` aligned: the proxy has a roughly seven-day security
cooldown, plus a one-day margin in this repository. Dependabot applies the
cooldown only to version updates, so a security update can propose a version
that the proxy does not serve yet.

## Configuration

- `.prettierrc` - Prettier configuration (formatting rules)
- `eslint.config.js` - ESLint configuration (code quality rules)
- `package.json` - Dependencies and scripts

## Best Practices

1. Run `npm run lint:format` before committing changes
2. Use double quotes for strings (enforced by Prettier)
3. Always use trailing commas in multiline objects/arrays
4. Use `const` instead of `let` when variables are not reassigned
5. Use template literals instead of string concatenation when possible
