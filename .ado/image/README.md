# CI/PR VM images

The Azure DevOps pipelines in this folder run on **1ES managed images** that are
shared with [microsoft/v8-jsi](https://github.com/microsoft/v8-jsi). The image
definitions — the ordered lists of provisioning artifacts that produce each image
— live there and are the single source of truth:

<https://github.com/microsoft/v8-jsi/tree/master/.ado/image>

They are intentionally **not** duplicated here: two copies of the same JSON would
drift, and only one of them can be the one that is actually built. Change the
image in v8-jsi; hermes-windows picks up the result automatically because the
pipelines select an image by name, not by version.

Both images carry the same toolchain — **Visual Studio 2026 Enterprise with
Clang, the current Windows SDK, Node.js 24, Python and .NET 10** — so x64, x86,
ARM64 and ARM64EC all build and test with the same tools.

## Pool to image mapping

| Cells | Pool | Image | How the image is selected |
|---|---|---|---|
| `win32_x64`, `win32_x86`, all `uwp_*` | `fabric-internal-pool-large` | `windows-2025-1espt` | `demands: ImageOverride -equals windows-2025-1espt` |
| `win32_arm64`, `win32_arm64ec` | `windows-2025-1espt-arm64` | `windows-2025-1espt-arm64` | none — single-image pool |

`fabric-internal-pool-large` hosts several images, so the image **must** be named
with an `ImageOverride` demand. This is set once on the top-level `pool:` in
[`build-template.yml`](../build-template.yml) and inherited by every job that
does not declare its own pool.

`windows-2025-1espt-arm64` is a **single-image pool** — its one default image is
`windows-2025-1espt-arm64` — so it takes no `ImageOverride` demand, and adding
one would only risk breaking agent matching.

## Why ARM64 and ARM64EC run on their own pool

ARM64 and ARM64EC code cannot execute on an x64 agent. On an x64 host
`.ado/scripts/build.js` treats both as cross builds: it builds only the shared
libraries and skips the test run entirely. On the native ARM64 pool both are
host-native, so the full target set (tools, unit-test binaries, lit drivers)
builds and the C++ unit tests, the JS regression tests and the Test262 Intl
tests all run against the binaries that actually ship.

ARM64EC binaries run natively on Windows on ARM as well, and the ARM64EC test
executables are themselves ARM64EC, so they exercise the shipped ARM64EC
`hermes.dll` / `hermes-icu.dll` the way a real consumer would.

## Updating an image

Edit the JSON in v8-jsi and trigger a managed-image rebuild there. A branch edit
alone changes nothing: a pipeline run always uses the currently *published*
image, and rebuilding plus replicating one takes hours. Rebuild the image before
relying on a new tool or environment variable in a pipeline step here.
