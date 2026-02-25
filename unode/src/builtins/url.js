'use strict';

const path = require('path');
const primordials = require('internal/util/primordials');
const { internalBinding } = require('internal/test/binding_runtime');
globalThis.primordials = Object.assign({}, globalThis.primordials || {}, primordials);
if (typeof globalThis.internalBinding !== 'function') {
  globalThis.internalBinding = internalBinding;
}

module.exports = require(path.resolve(__dirname, '../../../node/lib/url.js'));
