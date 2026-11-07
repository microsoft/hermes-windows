// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

import fs from "node:fs";

// Reviewed post-sync export inventory: 170 hosting C, 137 Static Hermes, 3 hoost.
// Exact names are both required and allowed; additions need allowlist review.
// _sh_model represents the arch/config-specific model function, not a wildcard.
export const requiredExports = fs
  .readFileSync(new URL("./hermes-exports.txt", import.meta.url), "utf8")
  .trim()
  .split(/\s+/);

type Section = { start: number; end: number; executable: boolean };

// DUMPBIN does not reliably label DATA: verify each RVA against PE section flags.
export function readSections(image: Buffer): Section[] {
  if (image.length < 0x40 || image.toString("ascii", 0, 2) !== "MZ") {
    throw new Error("Invalid PE image");
  }
  const pe = image.readUInt32LE(0x3c);
  if (pe > image.length - 24 || image.readUInt32LE(pe) !== 0x4550) {
    throw new Error("Invalid PE image");
  }
  const count = image.readUInt16LE(pe + 6);
  const table = pe + 24 + image.readUInt16LE(pe + 20);
  if (!count || table + count * 40 > image.length) {
    throw new Error("Invalid PE section table");
  }
  return Array.from({ length: count }, (_, i) => {
    const offset = table + i * 40;
    const start = image.readUInt32LE(offset + 12);
    return {
      start,
      end: start + image.readUInt32LE(offset + 8),
      executable: !!(image.readUInt32LE(offset + 36) & 0x20000000),
    };
  });
}

export function checkExports(
  output: string,
  platform: string,
  sections: Section[],
  required: string[] = requiredExports,
): number {
  const header = /^\s*ordinal\s+hint\s+RVA\s+name\s*$/m.exec(output);
  const functions = /^\s*(\d+) number of functions\s*$/m.exec(output);
  const names = /^\s*(\d+) number of names\s*$/m.exec(output);
  const summary = /^\s*Summary\s*$/m.exec(output);
  if (
    !header ||
    !functions ||
    !names ||
    !summary ||
    summary.index <= header.index ||
    !Number(functions[1])
  ) {
    throw new Error("Empty, zero-export or unparseable dumpbin export table");
  }
  const errors: string[] = [];
  const found = new Set<string>();
  const rows = output
    .slice(header.index + header[0].length, summary.index)
    .trim()
    .split(/\r?\n/)
    .filter((line) => line.trim());
  for (const row of rows) {
    // Reject NONAME and forwarders even when there is no RVA/hint column.
    if (/\b(?:NONAME|DATA)\b|\[NONAME\]|\(forwarded to /i.test(row)) {
      errors.push(`Forbidden DATA, NONAME or forwarder export: ${row.trim()}`);
      continue;
    }
    const match =
      /^\s*\d+\s+[\da-f]+\s+([\da-f]{8})\s+(\S+)(?:\s+=\s+\S+)?\s*$/i.exec(row);
    if (!match) {
      errors.push(`Unparseable/ordinal-only/forwarder export: ${row.trim()}`);
      continue;
    }
    const [, rvaText, name] = match;
    const rva = parseInt(rvaText, 16);
    if (!sections.some((s) => s.executable && rva >= s.start && rva < s.end)) {
      errors.push(`DATA or invalid export RVA: ${name}`);
    }
    if (/^(?:\?|_Z)/.test(name)) {
      errors.push(`C++ export: ${name}`);
    }
    const canonical = name.replace(
      /^_sh_model_s22_p[48]_(?:dbg|rel)$/,
      "_sh_model",
    );
    if (found.has(canonical)) {
      errors.push(`Duplicate export/model: ${name}`);
    }
    found.add(canonical);
    if (!required.includes(canonical) || name === "_sh_model") {
      errors.push(`Unknown export: ${name}; add to allowlist after review`);
    }
  }
  if (rows.length !== Number(functions[1]) || found.size !== Number(names[1])) {
    errors.push("Export counts disagree (ordinal-only or malformed table)");
  }
  for (const name of required) {
    if (platform === "arm64ec" && name.startsWith("hoost_")) {
      continue;
    }
    if (!found.has(name)) {
      errors.push(`Missing required export: ${name}`);
    }
  }
  if (errors.length) {
    throw new Error(errors.join("\n"));
  }
  return found.size;
}

export function checkDllExports(
  dll: string,
  platform: string,
  dumpbin: (dll: string) => string,
): number {
  try {
    const output = dumpbin(dll); // Tool errors must propagate, never become a pass.
    return checkExports(output, platform, readSections(fs.readFileSync(dll)));
  } catch (error) {
    throw new Error(`hermes.dll export check failed: ${dll}\n${error.message}`);
  }
}
