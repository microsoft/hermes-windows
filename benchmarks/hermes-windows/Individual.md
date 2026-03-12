# Individual Benchmarks

Standalone JS files that can be run directly with `hermes` (not through bench-runner.py).
Files requiring Static Hermes (`shermes`) or Flow type annotations do not work with `hermes.exe` and are excluded.

## micros/

- micros/getNodeById.js: `Time: {ms}`
- micros/setInsert.js: `Time: {ms}`
- micros/stringify-number.js: `Time: {ms}`
- micros/typed-array-sort.js: `{TypeName}: {ms} ms` (one line per typed array type, e.g. `Int8Array       : 1239 ms`)

## jit-benches/

- jit-benches/idisp.js: `Time: {ms}`
- jit-benches/idispn.js: `Time: {ms}`

## many-subclasses/

- many-subclasses/many.js: `{result} M={num-subclasses}, N={iterations}, time={ms}`

## map-objects/

- map-objects/map-objects-untyped.js: `{ms} ms {N} iterations`

## map-strings/

- map-strings/map-strings-untyped.js: `{ms} ms {N} iterations`

## nbody/

- nbody/original/nbody.js: `{energy-value}` (correctness check only, no timing output)

## string-switch/

- string-switch/plain/bench.js: `Switch {ms}`, `Object {ms}`, `Map    {ms}` (three lines comparing approaches)

## raytracer/

- raytracer/original/bench-raytracer.js: `exec time:  {ms} ms`

## octane/ (Google Octane v2, BenchmarkSuite scoring)

- octane/box2d.js: `Box2D {score}`
- octane/crypto.js: `Crypto {score}`
- octane/deltablue.js: `DeltaBlue {score}`
- octane/earley-boyer.js: `EarleyBoyer {score}`
- octane/gbemu.js: `Gameboy {score}`
- octane/mandreel.js: `Mandreel {score}`
- octane/mandreel_latency.js: `MandreelLatency {score}`
- octane/navier-stokes.js: `NavierStokes {score}`
- octane/pdfjs.js: `PdfJS {score}`
- octane/raytrace.js: `RayTrace {score}`
- octane/regexp.js: `RegExp {score}`
- octane/richards.js: `Richards {score}`
- octane/splay.js: `Splay {score}`
- octane/splay_latency.js: `SplayLatency {score}`
- octane/typescript.js: `Typescript {score}`
- octane/zlib.js: `zlib {score}`

Note: Octane scores are higher = faster (inverse of time). Some octane benchmarks emit compiler warnings to stderr but still run correctly.
