import { readFileSync, writeFileSync } from 'node:fs';
import { relative, resolve } from 'node:path';
import { parseArgs } from 'node:util';

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

const { values } = parseArgs({
  options: {
    input: { type: 'string', short: 'i', multiple: true },
    output: { type: 'string', short: 'o' },
  },
});

const inputs = values.input;
const output = values.output;

if (!inputs || inputs.length === 0) {
  console.error('Error: at least one -i <input.json> is required.');
  console.error('Usage: report.ts -i <input.json> [-i <input2.json> ...] -o <output.md>');
  process.exit(1);
}

if (!output) {
  console.error('Error: exactly one -o <output.md> is required.');
  console.error('Usage: report.ts -i <input.json> [-i <input2.json> ...] -o <output.md>');
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
// Load all inputs
// ---------------------------------------------------------------------------

const datasets: BenchData[] = inputs.map(
  (f) => JSON.parse(readFileSync(f, 'utf-8')) as BenchData,
);

const runtimeNames = datasets.map((d, i) => d.runtime || `input-${i + 1}`);

// ---------------------------------------------------------------------------
// Collect all benchmark names and group them
// ---------------------------------------------------------------------------

function groupOf(name: string): string {
  if (name.includes('/')) return name.split('/')[0];
  if (name.startsWith('v8-')) return 'v8';
  return 'test-suites';
}

// Ordered groups and ordered benchmark names within each group.
const groupOrder: string[] = [];
const groupBenchmarks = new Map<string, string[]>();

for (const data of datasets) {
  for (const name of Object.keys(data.results)) {
    const g = groupOf(name);
    if (!groupBenchmarks.has(g)) {
      groupBenchmarks.set(g, []);
      groupOrder.push(g);
    }
    const list = groupBenchmarks.get(g)!;
    if (!list.includes(name)) list.push(name);
  }
}

// Build a quick lookup: datasetIndex -> benchmarkName -> mean
const meanLookup: Map<string, number>[] = datasets.map((d) => {
  const m = new Map<string, number>();
  for (const [name, result] of Object.entries(d.results)) {
    m.set(name, result.totalTime.mean);
  }
  return m;
});

// Split a benchmark name into path components on either separator.
const PATH_SEP = /[\/\\]/;

// Longest path prefix shared by every name in `names`. Returns the shared
// components as an array (empty when there is nothing to factor out). The
// last path component of each name is always left in place so row cells
// are never emptied out.
function commonPathPrefix(names: string[]): string[] {
  if (names.length === 0) return [];
  const split = names.map((n) => n.split(PATH_SEP));
  const minLen = Math.min(...split.map((s) => s.length));
  if (minLen < 2) return [];
  const maxPrefix = minLen - 1;
  const prefix: string[] = [];
  for (let i = 0; i < maxPrefix; i++) {
    const first = split[0][i];
    if (split.every((s) => s[i] === first)) {
      prefix.push(first);
    } else {
      break;
    }
  }
  return prefix;
}

function stripPathPrefix(name: string, prefixLen: number): string {
  if (prefixLen === 0) return name;
  return name.split(PATH_SEP).slice(prefixLen).join('/');
}

// ---------------------------------------------------------------------------
// Generate markdown
// ---------------------------------------------------------------------------

// Render a single group section. Used for both single-input (one runtime
// column) and comparison (multiple runtime columns) modes — the only
// difference is how many value columns there are and whether the row
// minimum is bolded.
function renderSection(group: string, benchNames: string[]): string[] {
  const out: string[] = [];

  // Per-dataset sums of all benchmark means in this group, used in the
  // foldable title.
  const sums: number[] = meanLookup.map((lookup) => {
    let s = 0;
    for (const name of benchNames) {
      const v = lookup.get(name);
      if (v !== undefined) s += v;
    }
    return s;
  });
  const sumStr = sums.map((s) => `${s}ms`).join(', ');

  // Factor out the longest path prefix shared by every name in this group.
  // When present, the prefix is stripped from each row cell. The header
  // cell shows `(<prefix>)` only when it carries new info beyond the
  // group name — if prefix == group the annotation is redundant.
  const prefix = commonPathPrefix(benchNames);
  const prefixStr = prefix.join('/');
  const titleCell = prefix.length > 0 && prefixStr !== group
    ? `${group} (${prefixStr})`
    : group;

  // Foldable section with a heading-styled summary, e.g.
  //   <details><summary><h2>v8 (3256ms)</h2></summary>
  out.push(`<details>`);
  out.push(`<summary><strong>${group} (${sumStr})</strong></summary>`);
  out.push('');

  // Table header: column 1 = group title (with optional path annotation),
  // remaining columns = one per dataset/runtime.
  const header = [titleCell, ...runtimeNames];
  out.push('| ' + header.join(' | ') + ' |');
  out.push('|---|' + runtimeNames.map(() => '---:|').join(''));

  // One benchmark per row.
  const compare = datasets.length > 1;
  for (const name of benchNames) {
    const vals = meanLookup.map((lookup) => lookup.get(name));
    const cells: string[] = [stripPathPrefix(name, prefix.length)];
    if (compare) {
      const defined = vals.filter((v): v is number => v !== undefined);
      const min = defined.length > 0 ? Math.min(...defined) : undefined;
      for (const v of vals) {
        if (v === undefined) cells.push('-');
        else if (v === min) cells.push(`**${v}ms**`);
        else cells.push(`${v}ms`);
      }
    } else {
      for (const v of vals) {
        cells.push(v === undefined ? '-' : `${v}ms`);
      }
    }
    out.push('| ' + cells.join(' | ') + ' |');
  }

  out.push('');
  out.push(`</details>`);
  return out;
}

const lines: string[] = [];

lines.push(`# Benchmark Results`);
lines.push('');

const totalCount = new Set(datasets.flatMap((d) => Object.keys(d.results))).size;
lines.push(`**Total benchmarks:** ${totalCount}`);
lines.push('');

if (datasets.length > 1) {
  // Show input paths relative to this script's folder
  // (benchmarks/hermes-windows). Runtime labels are often identical
  // across inputs in real runs, so they don't help disambiguate.
  const reportDir = import.meta.dirname;
  const inputPaths = inputs.map((p) =>
    relative(reportDir, resolve(p)).replaceAll('\\', '/'),
  );
  lines.push(`**Inputs:** ${inputPaths.join(', ')}`);
  lines.push('');
}

for (const group of groupOrder) {
  const benchNames = groupBenchmarks.get(group)!;
  lines.push(...renderSection(group, benchNames));
}

const md = lines.join('\n');
writeFileSync(output, md);
console.log(`Report written to: ${output}`);
