// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { test } from "node:test";
import { Buffer } from "node:buffer";
import {
  checkDllExports,
  checkExports,
  readSections,
  requiredExports,
} from "./check-exports.ts";

const required = ["napi_example", "_sh_model", "hoost_jump_fcontext"];
const sections = [{ start: 0x1000, end: 0x2000, executable: true }];
const cleanRows = [
  "1 0 00001000 napi_example = napi_example",
  "2 1 00001010 _sh_model_s22_p8_dbg",
  "3 2 00001020 hoost_jump_fcontext",
];

// Generate only the table envelope to keep corruption fixtures tiny and readable.
function output(
  rows = cleanRows,
  functions = rows.length,
  names = rows.length,
) {
  return `Dump of file hermes.dll
File Type: DLL
  ${functions} number of functions
  ${names} number of names
  ordinal hint RVA      name
${rows.join("\n")}
  Summary
  1000 .text
`;
}
const check = (text = output(), platform = "x64") =>
  checkExports(text, platform, sections, required);

test("clean named function exports pass", () => {
  assert.equal(check(), 3);
});

// Each corruption exercises a separate forbidden PE export shape.
for (const [behavior, row, message] of [
  ["MSVC C++ name", "1 0 00001000 ?factory@@YAXXZ", /C\+\+ export: \?factory/],
  ["Itanium C++ name", "1 0 00001000 _Z7factoryv", /C\+\+ export: _Z/],
  ["DATA annotation", "1 0 00001000 napi_example DATA", /Forbidden DATA/],
  ["NONAME", "1 00001000 [NONAME]", /NONAME/],
  ["ordinal-only", "1 00001000", /ordinal-only/],
  ["forwarder", "1 0 napi_example (forwarded to other.function)", /forwarder/],
  ["unknown C name", "1 0 00001000 napi_new", /napi_new; add to allowlist/],
  [
    "unsupported model",
    "1 0 00001000 _sh_model_s22_p16_rel",
    /add to allowlist/,
  ],
  ["literal model placeholder", "1 0 00001000 _sh_model", /add to allowlist/],
  ["malformed row", "not an export", /Unparseable/],
]) {
  test(`${behavior} export fails with its offending name or row`, () => {
    assert.throws(() => check(output([row, ...cleanRows.slice(1)])), message);
  });
}

test("non-executable RVA fails even without a DATA annotation", () => {
  assert.throws(
    () => check(output(["1 0 00002000 napi_example", ...cleanRows.slice(1)])),
    /DATA or invalid export RVA: napi_example/,
  );
});

test("missing hosting or Static Hermes name fails", () => {
  for (const index of [0, 1]) {
    assert.throws(
      () => check(output(cleanRows.filter((_, i) => i !== index))),
      new RegExp(`Missing required export: ${required[index]}`),
    );
  }
});

test("missing hoost fails outside ARM64EC", () => {
  for (const arch of ["x64", "x86", "arm64"]) {
    assert.throws(
      () => check(output(cleanRows.slice(0, 2)), arch),
      /Missing required export: hoost_jump_fcontext/,
    );
  }
});

test("ARM64EC may omit hoost but still requires hosting and Static Hermes", () => {
  assert.equal(check(output(cleanRows.slice(0, 2)), "arm64ec"), 2);
  assert.throws(
    () => check(output([cleanRows[0]]), "arm64ec"),
    /Missing required export: _sh_model/,
  );
});

// Pointer size and debug/release are the only supported model-name variations.
for (const variant of ["p4_dbg", "p4_rel", "p8_dbg", "p8_rel"]) {
  test(`Static Hermes model ${variant} passes`, () => {
    assert.equal(check(output().replace("p8_dbg", variant)), 3);
  });
}

test("duplicate names and model variants fail", () => {
  // Both exact duplicates and model aliases must occupy only one export slot.
  for (const row of [cleanRows[0], "4 3 00001030 _sh_model_s22_p4_rel"]) {
    assert.throws(
      () => check(output([...cleanRows, row])),
      /Duplicate export\/model/,
    );
  }
});

// Truncation, count mismatches and empty output must all fail closed.
for (const [behavior, text] of [
  ["empty output", ""],
  ["arbitrary output", "tool succeeded but no export table"],
  ["zero exports", output([])],
  ["missing summary", output().replace("Summary", "truncated")],
  ["missing header", output().replace("ordinal hint RVA      name", "")],
  ["function count mismatch", output(cleanRows, 4)],
  ["name count mismatch", output(cleanRows, 3, 4)],
]) {
  test(`${behavior} fails closed`, () => assert.throws(() => check(text)));
}

// A 256-byte synthetic PE is enough to exercise the real file/section boundary.
function image() {
  const data = Buffer.alloc(256);
  data.write("MZ");
  data.writeUInt32LE(64, 0x3c);
  data.writeUInt32LE(0x4550, 64);
  data.writeUInt16LE(1, 70);
  data.writeUInt32LE(0x1000, 96); // VirtualSize of section at 88.
  data.writeUInt32LE(0x1000, 100); // VirtualAddress.
  data.writeUInt32LE(0x20000000, 124); // IMAGE_SCN_MEM_EXECUTE.
  return data;
}

test("PE section flags distinguish executable exports from data", () => {
  const data = image();
  assert.deepEqual(readSections(data), sections);
  data.writeUInt32LE(0x40000000, 124);
  assert.equal(readSections(data)[0].executable, false);
  assert.throws(() => readSections(Buffer.alloc(256)), /Invalid PE/);
  for (const length of [0, 2, 63]) {
    assert.throws(() => readSections(Buffer.alloc(length)), /Invalid PE image/);
  }
  data.writeUInt32LE(255, 0x3c);
  assert.throws(() => readSections(data), /Invalid PE image/);
  data.writeUInt32LE(64, 0x3c);
  data.writeUInt16LE(20, 70);
  assert.throws(() => readSections(data), /Invalid PE section table/);
});

test("dumpbin errors fail the DLL check with the file and tool error", () => {
  assert.throws(
    () =>
      checkDllExports("example.dll", "x64", () => {
        throw new Error("dumpbin exited 7");
      }),
    /example.dll\ndumpbin exited 7/,
  );
});

test("real file boundary rejects data exports without DATA text", () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "hermes-exports-"));
  const dll = path.join(dir, "hermes.dll");
  try {
    const data = image();
    data.writeUInt32LE(0x40000000, 124);
    fs.writeFileSync(dll, data);
    assert.throws(
      () => checkDllExports(dll, "x64", () => output()),
      /export check failed:[\s\S]*DATA or invalid export RVA: napi_example/,
    );
  } finally {
    fs.rmSync(dir, { recursive: true });
  }
});

test("reviewed inventory contains 310 names", () => {
  assert.equal(requiredExports.length, 310);
});
