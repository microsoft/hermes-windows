// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// Run from tools/hcdp after `npm ci`: node --test

const assert = require('node:assert/strict');
const {test} = require('node:test');
const WebSocket = require('ws');
const {
  DEVTOOLS_FRONTEND_ORIGIN,
  createServerOptions,
  isAllowedOrigin,
} = require('./origin');

/// Starts a server with the production options on an ephemeral port.
/// Resolves to {server, port, connections}.
function startServer() {
  return new Promise((resolve) => {
    const server = new WebSocket.Server(createServerOptions(0));
    const connections = [];
    server.on('connection', (ws) => connections.push(ws));
    server.on('listening', () =>
      resolve({server, port: server.address().port, connections}),
    );
  });
}

/// Attempts a handshake. Resolves to the HTTP status on rejection, or 101 if
/// the connection was accepted.
function tryConnect(port, origin) {
  return new Promise((resolve) => {
    const options = origin === undefined ? {} : {headers: {Origin: origin}};
    const client = new WebSocket(`ws://127.0.0.1:${port}`, options);
    client.on('open', () => {
      client.close();
      resolve(101);
    });
    client.on('unexpected-response', (req, res) => {
      resolve(res.statusCode);
      req.destroy();
    });
    client.on('error', () => {});
  });
}

test('listens on loopback only', async () => {
  const {server} = await startServer();
  try {
    assert.equal(server.address().address, '127.0.0.1');
  } finally {
    server.close();
  }
});

test('accepts the DevTools frontend origin', async () => {
  const {server, port, connections} = await startServer();
  try {
    assert.equal(await tryConnect(port, DEVTOOLS_FRONTEND_ORIGIN), 101);
    assert.equal(connections.length, 1);
  } finally {
    server.close();
  }
});

test('accepts native clients that send no origin', async () => {
  const {server, port, connections} = await startServer();
  try {
    assert.equal(await tryConnect(port, undefined), 101);
    assert.equal(connections.length, 1);
  } finally {
    server.close();
  }
});

test('rejects other origins before the connection is accepted', async () => {
  const rejected = [
    'http://untrusted.example',
    'null',
    '',
    'http://chrome-devtools-frontend.appspot.com',
    'https://chrome-devtools-frontend.appspot.com:8443',
    'https://chrome-devtools-frontend.appspot.com.evil.example',
    'https://evil-chrome-devtools-frontend.appspot.com',
    'https://appspot.com',
    'HTTPS://CHROME-DEVTOOLS-FRONTEND.APPSPOT.COM.evil.example',
  ];
  const {server, port, connections} = await startServer();
  try {
    for (const origin of rejected) {
      assert.equal(await tryConnect(port, origin), 401, origin);
    }
    assert.equal(connections.length, 0);
  } finally {
    server.close();
  }
});

test('isAllowedOrigin', () => {
  assert.equal(isAllowedOrigin(undefined), true);
  assert.equal(isAllowedOrigin(DEVTOOLS_FRONTEND_ORIGIN), true);
  assert.equal(isAllowedOrigin(`${DEVTOOLS_FRONTEND_ORIGIN}/`), false);
  assert.equal(isAllowedOrigin('null'), false);
});
