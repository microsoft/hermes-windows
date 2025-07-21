# JavaScript Build Scripts

This folder contains the JavaScript build scripts for Hermes Windows, along with tooling to maintain code quality.

## Files

- `build.js` - Main build script for Hermes Windows
- `setVersionNumber.js` - Script to set version numbers

## Code Quality Tools

We use **Prettier** and **ESLint** to maintain consistent code style and catch potential issues.

### Setup

From this directory (`.ado/scripts`), install the dependencies:

```bash
npm install
```

### Available Scripts

- `npm run format` - Format all JavaScript files with Prettier
- `npm run format:check` - Check if files need formatting
- `npm run lint` - Run ESLint to check for code issues
- `npm run lint:fix` - Run ESLint and automatically fix issues
- `npm run lint:format` - Format with Prettier and then run ESLint with auto-fix

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
