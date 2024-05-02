'use strict';
// Flags: --expose-gc

const { gcUntil, buildType } = require('../../common');
const assert = require('assert');

const test_finalizer = require(`./build/${buildType}/test_finalizer`);

// This test script uses external values with finalizer callbacks
// in order to track when values get garbage-collected. Each invocation
// of a finalizer callback increments the finalizeCount property.
assert.strictEqual(test_finalizer.finalizeCount, 0);

let objects = [];

function createObjects() {
  objects = [];
  for (let i = 0; i < 50000; i++) {
    const obj = new Object();
    test_finalizer.addPropertyWithFinalizer(obj);
    objects.push(obj);
  }
}

createObjects();

function runNextTick() {
  return new Promise((resolve, _) => {
    setImmediate(() => {
      resolve();
    });
  });
}

async function waitForBGGC() {
  for (let i = 0; i < 2; ++i) {
    await runNextTick();
    createObjects();
  }
}

waitForBGGC();

