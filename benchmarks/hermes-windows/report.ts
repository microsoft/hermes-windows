import { readFileSync, writeFileSync } from 'node:fs';
import { parseArgs } from 'node:util';

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

const { values } = parseArgs({
  options: {
    input: { type: 'string', short: 'i' },
    output: { type: 'string', short: 'o' },
  },
});

const input = values.input;
const output = values.output;

if (!input || !output) {
  console.error('Usage: report.ts -i <input.json> -o <output.md>');
  process.exit(1);
}

// ---------------------------------------------------------------------------
// Interfaces
// ---------------------------------------------------------------------------

interface BenchResult {
  totalTime: {
    mean: number;
    stdev: number;
    samples: number[];
  };
}

interface BenchData {
  runtime?: string;
  results: { [key: string]: BenchResult };
}

// ---------------------------------------------------------------------------
// Read and group
// ---------------------------------------------------------------------------

const data: BenchData = JSON.parse(readFileSync(input, 'utf-8'));
const results = data.results;

// Group benchmarks: entries with '/' use first directory, others go by prefix.
interface Group {
  name: string;
  entries: [string, number][];
}

const groupMap = new Map<string, [string, number][]>();

for (const [name, result] of Object.entries(results)) {
  let group: string;
  if (name.includes('/')) {
    group = name.split('/')[0];
  } else if (name.startsWith('v8-')) {
    group = 'v8';
  } else {
    // octane + micros from bench-runner all end up here
    group = 'test-suites';
  }
  if (!groupMap.has(group)) groupMap.set(group, []);
  groupMap.get(group)!.push([name, result.totalTime.mean]);
}

// ---------------------------------------------------------------------------
// Generate markdown
// ---------------------------------------------------------------------------

const lines: string[] = [];

lines.push(`# Benchmark Results`);
lines.push('');
if (data.runtime) {
  lines.push(`**Runtime:** ${data.runtime}`);
  lines.push('');
}

const totalCount = Object.keys(results).length;
lines.push(`**Total benchmarks:** ${totalCount}`);
lines.push('');

for (const [group, entries] of groupMap) {
  lines.push(`## ${group}`);
  lines.push('');
  lines.push('| Benchmark | Mean (ms) |');
  lines.push('|-----------|----------:|');
  for (const [name, mean] of entries) {
    lines.push(`| ${name} | ${mean} |`);
  }
  lines.push('');
}

const md = lines.join('\n');
writeFileSync(output, md);
console.log(`Report written to: ${output}`);
