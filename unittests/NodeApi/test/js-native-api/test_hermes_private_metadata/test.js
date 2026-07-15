'use strict';
// Flags: --expose-gc

const { buildType } = require('../../common');
const { gcUntil } = require('../../common/gc');
const assert = require('assert');

const metadata = require(
  `./build/${buildType}/test_hermes_private_metadata`);

function makeThrowingProxy() {
  let trapCount = 0;
  const proxy = new Proxy({}, {
    get() {
      trapCount++;
      throw new Error('unexpected get trap');
    },
    getOwnPropertyDescriptor() {
      trapCount++;
      throw new Error('unexpected descriptor trap');
    },
    defineProperty() {
      trapCount++;
      throw new Error('unexpected define trap');
    },
    deleteProperty() {
      trapCount++;
      throw new Error('unexpected delete trap');
    },
    has() {
      trapCount++;
      throw new Error('unexpected has trap');
    },
  });
  return { proxy, getTrapCount: () => trapCount };
}

{
  const { proxy, getTrapCount } = makeThrowingProxy();
  assert.strictEqual(metadata.wrapRoundTrip(proxy), true);
  assert.strictEqual(getTrapCount(), 0);
}

{
  const object = Object.freeze({});
  assert.strictEqual(metadata.wrapRoundTrip(object), true);
}

{
  const parent = {};
  const child = Object.create(parent);
  assert.strictEqual(metadata.wrapPrototypeIsolation(parent, child), true);
}

{
  const { proxy, getTrapCount } = makeThrowingProxy();
  assert.strictEqual(metadata.tagWrapAndCheck(proxy), true);
  assert.strictEqual(getTrapCount(), 0);
}

async function testProxyFinalizer() {
  const { getTrapCount } = (() => {
    const { proxy, getTrapCount } = makeThrowingProxy();
    metadata.addFinalizer(proxy);
    return { getTrapCount };
  })();

  await gcUntil(
    'Node-API finalizer on proxy',
    () => metadata.finalizeCount === 1);
  assert.strictEqual(getTrapCount(), 0);
}

testProxyFinalizer();
