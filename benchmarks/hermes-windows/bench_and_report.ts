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
const resultJson = join(__dirname, 'bench_result.json');
const reportMd = join(__dirname, 'bench_report.md');

// Step 1: Run benchmarks
console.log('=== Running benchmarks ===');
const benchArgs = ['--experimental-strip-types', benchScript, '--dynamic', '-c', '5', '-l', 'CI', '-o', resultJson];
if (values.binary) {
  benchArgs.push('--binary', values.binary);
}
const benchResult = spawnSync(
  process.execPath,
  benchArgs,
  { stdio: 'inherit' },
);

if (benchResult.status !== 0) {
  console.error(`bench.ts failed with exit code ${benchResult.status}`);
  process.exit(1);
}

// Stamp the result with the current time so any baseline copied from
// it is traceable back to the CI run that produced it.
const resultObj = JSON.parse(readFileSync(resultJson, 'utf8'));
resultObj.timestamp = new Date().toISOString();
writeFileSync(resultJson, JSON.stringify(resultObj, undefined, 4) + '\n');

// Step 2: Generate report. If --baseline <path> is given, render
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
