# Flow Bundler

This bundler allows providing any number of entry points then producing a single
"bundle" file that includes all entrypoints and any imported files. The bundler
does not do any lowering so maintains all Flow typing needed for Static Hermes.

## Usage

From the repository root, install the dependencies with the pinned Yarn release:

```
node .yarn/releases/yarn-4.13.0.cjs \
  --cwd benchmarks/build-helpers/flow-bundler install --immutable
```

Basic usage:

```
./benchmarks/build-helpers/flow-bundler/bin/flow-bundler \
  --out output/path/of/bundle.js
  --root project/root
  path/to/entrypoint1.js
  path/to/entrypoint2.js
```

For more detailed usage see "help":

```
./benchmarks/build-helpers/flow-bundler/bin/flow-bundler --help
```
