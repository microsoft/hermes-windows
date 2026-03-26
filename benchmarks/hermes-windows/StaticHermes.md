# Static Hermes (`--static`) Benchmark Support

## Current Status

The `--static` flag selects `shermes.exe` (Static Hermes) instead of `hermes.exe`.
Individual benchmarks work via `shermes -exec`, but test suites
(`bench-runner.py`) fail immediately.

## What Works

**Individual benchmarks** (`benchIndividual` / `benchIndividualSamples`) run
correctly. `shermes -exec` compiles JS to native code (via C + Clang) and
executes the result in one step. Tested flags: `-typed`, `-parse-flow`,
`-parse-ts` — all work.

Benchmarks that report their own timing (`Time: <ms>`, `time=<ms>`, etc.)
produce accurate results because the internal `Date.now()` measures only JS
execution, not compilation.

## What Fails

### 1. `benchTestSuites` — bench-runner.py is incompatible with shermes

`bench-runner.py` uses `HermesRunner` (in `runner.py`) which does:

1. **Compile**: `hermes -O -Wno-undefined-variable -emit-binary -out file.hbc file.js`
2. **Run**: `hermes -gc-print-stats -gc-sanitize-handles=0 -b file.hbc`

shermes does not support any of these flags:

| Flag | Purpose | shermes support |
|------|---------|-----------------|
| `-emit-binary` | Emit HBC bytecode | No (`Unknown command line argument`) |
| `-b` | Run bytecode file | No (shermes is a compiler, not an interpreter) |
| `-gc-print-stats` | Print GC stats | No (not a runtime flag for shermes) |
| `-gc-sanitize-handles` | GC safety checks | No |

shermes compiles JS to C then to a native executable — it has no bytecode
intermediate step. The bench-runner.py workflow is fundamentally designed around
the hermes bytecode model.

### 2. Wall-clock benchmarks include compilation overhead

Benchmarks without a parser (e.g., `nbody/original/nbody.js`,
`string-switch/plain/bench.js`) are timed with `Date.now()` in bench.ts,
wrapping the entire `spawnSync(shermes, ['-exec', ...])` call. This includes
shermes compilation time (~0.4 s overhead on a release build), making the
measurement unfair compared to hermes which interprets directly.

## Proposed Fixes

### Fix 1: Skip test suites in static mode

The simplest approach. In `benchAll`, skip `benchTestSuites` when
`mode === 'static'` and only run individual benchmarks. This avoids the
bench-runner.py incompatibility entirely.

Pros: Minimal code change, unblocks `--static` immediately.
Cons: No test-suite results for static hermes.

### Fix 2: Two-phase compile-then-run for individual benchmarks

Instead of `shermes -exec file.js` (which compiles + runs in one process),
split into two steps:

1. **Compile**: `shermes -O -o <tempdir>/bench.exe file.js`
2. **Run**: `<tempdir>/bench.exe`

This separates compilation time from execution time, giving accurate wall-clock
measurements for benchmarks that lack internal timing. The `-exec` flag
effectively does this internally, but we can't separate the timing.

### Fix 3: Add a `ShermesRunner` to bench-runner.py

Create a new runner class in `runner.py` alongside `HermesRunner`:

```python
class ShermesRunner(JSRunner):
    def __init__(self, shermesCmd, numTimes, keepTmpFiles):
        JSRunner.__init__(self, shermesCmd)
        self.numTimes = numTimes
        self.keepTmpFiles = keepTmpFiles

    def run(self, name, jsfile, gcMinHeap, gcInitHeap, gcMaxHeap):
        with tempfile.NamedTemporaryFile(suffix='.exe', delete=not self.keepTmpFiles) as exeFile:
            # Compile JS to native executable
            compileCmd = [self.runCmd, '-O', '-o', exeFile.name, jsfile]
            subprocess.run(compileCmd, check=True, ...)

            # Run the native executable
            execCmd = [exeFile.name]
            for _ in range(self.numTimes):
                proc = subprocess.run(execCmd, ...)
                yield parseSimpleStats(proc.stdout)
```

Then wire it up in `bench-runner.py` with a `--shermes` flag.

Pros: Enables full test-suite comparison between hermes and shermes.
Cons: Larger change, touches upstream-originated Python code.

## Recommended Approach

Apply Fix 1 + Fix 2 together as an initial step:
- Skip test suites in `--static` mode (quick unblock)
- Use two-phase compile-then-run for individual benchmarks (accurate timing)

Fix 3 (ShermesRunner) can be done later if test-suite comparison is needed.
