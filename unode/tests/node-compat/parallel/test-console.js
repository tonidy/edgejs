'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');

const {
  hijackStdout,
  hijackStderr,
  restoreStdout,
  restoreStderr
} = require('../common/hijackstdio');

assert.ok(process.stdout.writable);
assert.ok(process.stderr.writable);
if (common.isMainThread) {
  assert.strictEqual(typeof process.stdout.fd, 'number');
  assert.strictEqual(typeof process.stderr.fd, 'number');
}

common.expectWarning(
  'Warning',
  [
    ['Count for \'noLabel\' does not exist'],
    ['No such label \'noLabel\' for console.timeLog()'],
    ['No such label \'noLabel\' for console.timeEnd()'],
  ]
);

console.countReset('noLabel');
console.timeLog('noLabel');
console.timeEnd('noLabel');

const customInspect = { foo: 'bar', [util.inspect.custom]: () => 'inspect' };

const strings = [];
const errStrings = [];
process.stdout.isTTY = false;
hijackStdout(function(data) {
  strings.push(data);
});
process.stderr.isTTY = false;
hijackStderr(function(data) {
  errStrings.push(data);
});

console.log('foo');
console.log('foo', 'bar');
console.log('%s %s', 'foo', 'bar', 'hop');
console.log({ slashes: '\\\\' });
console.log(customInspect);

console.debug('foo');
console.info('foo');

console.error('foo');
console.warn('foo');

console.dir(customInspect);
console.dir({ foo: { bar: { baz: true } } }, { depth: 1 });
console.dirxml(customInspect, customInspect);

console.trace('This is a %j %d', { formatted: 'trace' }, 10, 'foo');

console.time('label');
console.timeEnd('label');

console.time('test');
const start = console._times.get('test');
setTimeout(() => {
  console.time('test');
  assert.deepStrictEqual(console._times.get('test'), start);
  console.timeEnd('test');
}, 1);

console.time('log1');
console.timeLog('log1');
console.timeLog('log1', 'test');
console.timeEnd('log1');

console.assert(false, '%s should', 'console.assert', 'not throw');
assert.strictEqual(errStrings[errStrings.length - 1], 'Assertion failed: console.assert should not throw\n');

console.assert(false);
assert.strictEqual(errStrings[errStrings.length - 1], 'Assertion failed\n');

console.assert(true, 'this should not throw');
console.assert(true);

assert.strictEqual(strings.length, process.stdout.writeTimes);
assert.strictEqual(errStrings.length, process.stderr.writeTimes);
restoreStdout();
restoreStderr();

assert.ok(strings.some((s) => s === 'foo\n'));
assert.ok(strings.some((s) => s.indexOf('foo bar') !== -1));
assert.ok(strings.some((s) => s.indexOf('inspect') !== -1));
assert.ok(strings.some((s) => /^label: \d+ms\n$/.test(s)));
assert.ok(strings.some((s) => /^log1: \d+ms/.test(s)));

assert.ok(errStrings.some((s) => s === 'foo\n'));
assert.ok(errStrings.some((s) => s.indexOf('Trace:') === 0));

hijackStderr(common.mustCall(function(data) {
  restoreStderr();
  process.nextTick(function() {
    assert.strictEqual(data.includes('noLabel'), true);
  });
}));
