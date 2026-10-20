// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

/// The only browser origin allowed to connect: the DevTools frontend that
/// hcdp.js opens.
const DEVTOOLS_FRONTEND_ORIGIN = 'https://chrome-devtools-frontend.appspot.com';

/// Returns true if a WebSocket handshake with the given Origin header value
/// may connect to the debugger.
///
/// Browsers always send an Origin header, so rejecting every unexpected value
/// stops arbitrary web pages from driving the debugger. A missing header means
/// a native (non-browser) CDP client, which is allowed. The literal "null"
/// origin (sandboxed pages, file: URLs) is rejected.
function isAllowedOrigin(origin) {
  if (origin === undefined) {
    return true;
  }
  return origin === DEVTOOLS_FRONTEND_ORIGIN;
}

/// Returns the WebSocket.Server options for the debugger: listen on loopback
/// only, and reject disallowed origins before the connection is accepted.
function createServerOptions(port) {
  return {
    host: '127.0.0.1',
    port: port,
    verifyClient: (info) => isAllowedOrigin(info.req.headers.origin),
  };
}

module.exports = {
  DEVTOOLS_FRONTEND_ORIGIN,
  isAllowedOrigin,
  createServerOptions,
};
