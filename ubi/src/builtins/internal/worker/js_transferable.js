'use strict';

const path = require('path');

try {
  module.exports = require(path.resolve(__dirname, '../../../../../node-lib/internal/worker/js_transferable.js'));
} catch {
  function markTransferMode() {}
  function setup() {}
  function structuredClone(value, options) {
    return globalThis.structuredClone(value, options);
  }

  module.exports = {
    markTransferMode,
    setup,
    structuredClone,
    kClone: Symbol('kClone'),
    kDeserialize: Symbol('kDeserialize'),
    kTransfer: Symbol('kTransfer'),
    kTransferList: Symbol('kTransferList'),
  };
}
