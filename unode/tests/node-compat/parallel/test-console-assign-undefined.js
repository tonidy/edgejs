'use strict';

const tmp = global.console;
global.console = 42;

require('../common');
const assert = require('assert');

assert.strictEqual(global.console, 42);
assert.strictEqual(global.console, 42);

assert.throws(
  () => console.log('foo'),
  { name: 'TypeError' }
);

global.console = 1;
assert.strictEqual(global.console, 1);
assert.strictEqual(console, 1);

global.console = tmp;
console.log('foo');
