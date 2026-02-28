'use strict';

// Re-export the builtin assert so tests can use require('../common/assert') or require('assert').
// Both resolve to the same implementation from unode/src/builtins.
module.exports = require('assert');
