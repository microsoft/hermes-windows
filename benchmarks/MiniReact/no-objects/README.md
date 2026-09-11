This is the `no-objects` MiniReact benchmark with objects and other unsupported
features removed.

# Setup

```
(cd ~/builds; cmake -S ~/fbsource/xplat/static_h/benchmarks/ -B benchmarksdebug -G Ninja -DHERMES_BUILD=~/builds/shdebug -DHERMES_SRC=~/fbsource/xplat/static_h) && (cd ~/fbsource/xplat/static_h; node .yarn/releases/yarn-4.13.0.cjs --cwd benchmarks/build-helpers/flow-bundler install --immutable)
```

# Run with Static Hermes

```
~/fbsource/xplat/static_h/benchmarks/MiniReact/no-objects/build.sh && (cd ~/builds/shdebug/; ninja hermesvm shermes && ./bin/shermes -typed -exec -g ~/fbsource/xplat/static_h/benchmarks/MiniReact/no-objects/out/simple.js)
```

# Run with Hermes

```
~/fbsource/xplat/static_h/benchmarks/MiniReact/no-objects/build.sh && (cd ~/builds/shdebug/; ninja hermesvm hermes && ./bin/hermes -exec -g ~/fbsource/xplat/static_h/benchmarks/MiniReact/no-objects/out/simple-lowered.js)
```
