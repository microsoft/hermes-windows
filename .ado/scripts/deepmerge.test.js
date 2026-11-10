// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

import assert from "node:assert/strict";
import { createRequire } from "node:module";
import path from "node:path";
import process from "node:process";
import { test } from "node:test";

// Each project's test script runs this file from its own directory, so this
// checks the deepmerge copy installed in that project.
const require = createRequire(path.join(process.cwd(), "package.json"));
const maliciousObject = () =>
  JSON.parse('{"__proto__":{"__deepmergeProbe":true},"kept":true}');
const attacks = [
  ["null-prototype target", () => [Object.create(null), maliciousObject()]],
  ["null placeholder", () => [{ value: null }, { value: maliciousObject() }]],
  [
    "undefined placeholder",
    () => [{ value: undefined }, { value: maliciousObject() }],
  ],
  [
    "primitive placeholder",
    () => [{ value: false }, { value: maliciousObject() }],
  ],
  ["own target __proto__", () => [maliciousObject(), { extra: true }]],
  [
    "own target and source __proto__",
    () => [maliciousObject(), maliciousObject()],
  ],
  ["array element", () => [[], [maliciousObject()]]],
];

for (const entry of ["deepmerge", "deepmerge/dist/umd.js"]) {
  const merge = require(entry);

  for (const [name, inputs] of attacks) {
    test(`${entry}: prevents prototype poisoning with ${name}`, () => {
      const [target, source] = inputs();
      const merged = merge(target, source);
      const result = Array.isArray(merged)
        ? merged[0]
        : (merged.value ?? merged);
      assert.equal(result.__deepmergeProbe, undefined);
      assert.equal(Object.hasOwn(result, "__proto__"), false);
      assert.equal(Object.getPrototypeOf(result), Object.prototype);
      assert.equal(result.kept, true);
      assert.equal(Object.prototype.__deepmergeProbe, undefined);
    });
  }

  test(`${entry}: preserves normal object and array merges and merge.all`, () => {
    const target = { nested: { left: true }, list: [{ left: true }] };
    const source = { nested: { right: true }, list: [{ right: true }] };
    const expected = {
      nested: { left: true, right: true },
      list: [{ left: true }, { right: true }],
    };
    assert.deepEqual(merge(target, source), expected);
    assert.deepEqual(merge.all([target, source, { last: true }]), {
      ...expected,
      last: true,
    });
    assert.deepEqual(target, {
      nested: { left: true },
      list: [{ left: true }],
    });
    assert.deepEqual(source, {
      nested: { right: true },
      list: [{ right: true }],
    });
  });

  test(`${entry}: preserves clone and arrayMerge options`, () => {
    const value = { kept: true };
    assert.equal(merge({}, { value }, { clone: false }).value, value);
    assert.deepEqual(
      merge([1], [2], { arrayMerge: (_target, source) => source }),
      [2],
    );
  });

  test(`${entry}: preserves legitimate own constructor and prototype keys`, () => {
    const target = { constructor: { left: true }, prototype: { left: true } };
    const source = { constructor: { right: true }, prototype: { right: true } };
    assert.deepEqual(merge(target, source), {
      constructor: { left: true, right: true },
      prototype: { left: true, right: true },
    });
  });
}
