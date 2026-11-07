/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @noflow
 * @format
 */

'use strict';

const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {createRequire} = require('node:module');

const from = (parent, name) => createRequire(parent.resolve(name));
const workspace = createRequire(path.resolve(__dirname, '..', 'package.json'));
const jestPackage = from(workspace, 'jest');
const core = from(from(jestPackage, 'jest-cli'), '@jest/core');
const config = from(core, 'jest-config');
// Resolve through Jest so hoisted dependencies from older Jest cannot mask it.
const istanbul = from(from(core, '@jest/transform'), 'babel-plugin-istanbul');
const coverage = from(istanbul, 'test-exclude');
const normalizeFiles = files =>
  files.map(file => file.replace(/\\/g, '/')).sort();

describe('tooling file discovery', () => {
  let fixture;

  beforeAll(() => {
    fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'hermes-tooling-glob-'));
    fs.mkdirSync(path.join(fixture, 'src', 'nested'), {recursive: true});
    for (const filename of [
      path.join('src', 'app.js'),
      path.join('src', 'app-test.js'),
      path.join('src', 'style.css'),
      path.join('src', 'nested', 'util.ts'),
    ]) {
      fs.writeFileSync(path.join(fixture, filename), '');
    }
  });

  afterAll(() => {
    fs.rmSync(fixture, {recursive: true, force: true});
  });

  it.each([
    ['coverage', coverage],
    ['Jest configuration', config],
  ])('%s discovers wildcard paths', (_name, parent) => {
    const files = parent('glob').globSync('src/**/*.{js,ts}', {cwd: fixture});
    expect(normalizeFiles(files)).toEqual([
      'src/app-test.js',
      'src/app.js',
      'src/nested/util.ts',
    ]);
  });

  it('coverage discovers source files and excludes test files', () => {
    const TestExclude = istanbul('test-exclude');
    const exclude = new TestExclude({
      cwd: fixture,
      include: ['src/**/*.{js,ts}'],
      exclude: ['**/*-test.js'],
    });
    expect(normalizeFiles(exclude.globSync(fixture))).toEqual([
      'src/app.js',
      'src/nested/util.ts',
    ]);
  });
});
