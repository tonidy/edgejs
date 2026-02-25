'use strict';

const path = require('path');

const fixturesDir = path.join(__dirname, '..', 'fixtures');

function fixturesPath(...args) {
  return path.join(fixturesDir, ...args);
}

module.exports = {
  path: fixturesPath,
};
