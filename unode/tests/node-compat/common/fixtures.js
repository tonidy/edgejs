'use strict';

const path = require('path');

// When running raw Node tests (NODE_TEST_DIR set), use node/test/fixtures so paths match Node.
const fixturesDir = typeof process !== 'undefined' && process.env && process.env.NODE_TEST_DIR
  ? path.join(process.env.NODE_TEST_DIR, 'fixtures')
  : path.join(__dirname, '..', 'fixtures');

function fixturesPath(...args) {
  return path.join(fixturesDir, ...args);
}

module.exports = {
  path: fixturesPath,
};
