import { readFileSync, writeFileSync } from 'node:fs';
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
const multiInput = datasets.length > 1;

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

const lines: string[] = [];

lines.push(`# Benchmark Results`);
lines.push('');

const totalCount = new Set(datasets.flatMap((d) => Object.keys(d.results))).size;
lines.push(`**Total benchmarks:** ${totalCount}`);
lines.push('');

for (const group of groupOrder) {
  const benchNames = groupBenchmarks.get(group)!;

  // Factor out the longest path prefix shared by every name in this group.
  // When present, the prefix is stripped from each row cell. The parenthetical
  // `(<prefix>)` is appended to the header only when it carries new info
  // beyond the group name — if prefix == group the annotation would be
  // redundant (`micros (micros)`), so omit it.
  const prefix = commonPathPrefix(benchNames);
  const prefixStr = prefix.join('/');
  const titleCell = prefix.length > 0 && prefixStr !== group
    ? `${group} (${prefixStr})`
    : group;

  if (multiInput) {
    // Compare mode: one column per runtime. Group+prefix goes in column 1
    // header (replacing the old `## <group>` heading + "Benchmark (ms)" label).
    const header = [titleCell, ...runtimeNames];
    lines.push('| ' + header.join(' | ') + ' |');
    lines.push(
      '|' +
        header.map((_, i) => (i === 0 ? '---|' : '---:|')).join(''),
    );

    for (const name of benchNames) {
      const vals = meanLookup.map((lookup) => lookup.get(name));
      const defined = vals.filter((v): v is number => v !== undefined);
      const min = defined.length > 0 ? Math.min(...defined) : undefined;
      const cells: string[] = [stripPathPrefix(name, prefix.length)];
      for (const v of vals) {
        if (v === undefined) cells.push('-');
        else if (v === min) cells.push(`**${v}ms**`);
        else cells.push(`${v}ms`);
      }
      lines.push('| ' + cells.join(' | ') + ' |');
    }
    lines.push('');
    continue;
  }

  // Single-input: pack up to 3 benchmarks per row (N1 T1 N2 T2 N3 T3).
  // If the group has fewer than 3 benchmarks total, shrink the column count
  // so no entire column is left empty.
  const lookup = meanLookup[0];
  const packWidth = Math.min(3, benchNames.length);
  const totalCols = packWidth * 2;

  // Header row: col 1 = group title (with common path), col 2 = runtime
  // label, rest empty.
  const headerCells: string[] = new Array(totalCols).fill('');
  headerCells[0] = titleCell;
  headerCells[1] = runtimeNames[0];
  lines.push('| ' + headerCells.join(' | ') + ' |');

  // Separator: name cols left-aligned, time cols right-aligned.
  const sep: string[] = [];
  for (let c = 0; c < totalCols; c++) {
    sep.push(c % 2 === 0 ? '---' : '---:');
  }
  lines.push('| ' + sep.join(' | ') + ' |');

  // Body rows, packed.
  for (let i = 0; i < benchNames.length; i += packWidth) {
    const row: string[] = [];
    for (let j = 0; j < packWidth; j++) {
      const idx = i + j;
      if (idx < benchNames.length) {
        const name = benchNames[idx];
        const v = lookup.get(name);
        row.push(stripPathPrefix(name, prefix.length));
        row.push(v === undefined ? '-' : `${v}ms`);
      } else {
        row.push('');
        row.push('');
      }
    }
    lines.push('| ' + row.join(' | ') + ' |');
  }
  lines.push('');
}

const md = lines.join('\n');
writeFileSync(output, md);
console.log(`Report written to: ${output}`);
