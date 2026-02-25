'use strict';

require('../common');

const assert = require('assert');
const { Console } = require('console');

const queue = [];

const consoleObj = new Console({ write: (x) => {
  queue.push(x);
}, removeListener: () => {} }, process.stderr, false);

consoleObj.table(null);
assert.strictEqual(queue.shift(), 'null\n');

consoleObj.table([1, 2, 3]);
assert.strictEqual(queue.shift(), '[ 1, 2, 3 ]\n');

consoleObj.table({ a: 1, b: 2 });
assert.strictEqual(queue.shift(), "{ a: 1, b: 2 }\n");

consoleObj.table(new Set([1, 2, 3]));
assert.ok(queue.shift().includes('1'));
