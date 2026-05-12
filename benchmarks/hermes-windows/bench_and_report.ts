import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const { values } = parseArgs({
  options: {
    binary: { type: 'string', short: 'b' },
    baseline: { type: 'string' },
  },
  strict: false,
});

const benchScript = join(__dirname, 'bench.ts');
const reportScript = join(__dirname, 'report.ts');
const dynamicJson = join(__dirname, 'bench_dynamic_result.json');
const staticJson = join(__dirname, 'bench_static_result.json');
const resultJson = join(__dirname, 'bench_result.json');
const reportMd = join(__dirname, 'bench_report.md');

interface BenchData {
  runtime?: string;
  results: { [key: string]: unknown };
  timestamp?: string;
}

function runBench(mode: '--dynamic' | '--static', output: string): void {
  const args = ['--experimental-strip-types', benchScript, mode, '-c', '5', '-l', 'CI', '-o', output];
  if (values.binary) {
    args.push('--binary', values.binary);
  }
  const r = spawnSync(process.execPath, args, { stdio: 'inherit' });
  if (r.status !== 0) {
    console.error(`bench.ts (${mode}) failed with exit code ${r.status}`);
    process.exit(1);
  }
}

// Step 1: Run dynamic benchmarks
console.log('=== Running dynamic benchmarks ===');
runBench('--dynamic', dynamicJson);

// Step 2: Run static benchmarks
console.log('');
console.log('=== Running static benchmarks ===');
runBench('--static', staticJson);

// Step 3: Merge dynamic + static into bench_result.json. Static benchmark
// names are suffixed with " (static)" so they coexist with dynamic results
// in the same groups. Runtime is taken from the dynamic run; static's
// runtime field is intentionally dropped.
const dynamicObj = JSON.parse(readFileSync(dynamicJson, 'utf8')) as BenchData;
const staticObj = JSON.parse(readFileSync(staticJson, 'utf8')) as BenchData;

const merged: BenchData = {
  runtime: dynamicObj.runtime,
  results: { ...dynamicObj.results },
};
for (const [name, value] of Object.entries(staticObj.results)) {
  merged.results[`${name} (static)`] = value;
}
merged.timestamp = new Date().toISOString();
writeFileSync(resultJson, JSON.stringify(merged, undefined, 4) + '\n');

// Step 4: Generate report. If --baseline <path> is given, render
// comparison mode with baseline first and the new result second.
console.log('');
console.log('=== Generating report ===');
const reportArgs = ['--experimental-strip-types', reportScript];
if (values.baseline) {
  reportArgs.push('-i', values.baseline, '-i', resultJson);
} else {
  reportArgs.push('-i', resultJson);
}
reportArgs.push('-o', reportMd);
const reportResult = spawnSync(
  process.execPath,
  reportArgs,
  { stdio: 'inherit' },
);

if (reportResult.status !== 0) {
  console.error(`report.ts failed with exit code ${reportResult.status}`);
  process.exit(1);
}

console.log('');
console.log('Done.');
