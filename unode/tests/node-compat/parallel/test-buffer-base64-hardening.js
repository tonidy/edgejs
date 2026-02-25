'use strict';

const assert = require('assert');
const { Buffer } = require('buffer');

assert.strictEqual(Buffer.from('YQ==', 'base64').toString('utf8'), 'a');
assert.strictEqual(Buffer.from('YQ', 'base64').toString('utf8'), 'a');
assert.strictEqual(Buffer.from(' Y Q = = \n', 'base64').toString('utf8'), 'a');
assert.strictEqual(Buffer.from('YQ$%', 'base64').toString('utf8'), 'a');
assert.strictEqual(Buffer.from('YW=Jj', 'base64').toString('utf8'), 'a');
assert.strictEqual(Buffer.from('Y===', 'base64').toString('utf8'), '');
assert.strictEqual(Buffer.from('Y W\nQ==', 'base64').toString('utf8'), 'ad');
assert.strictEqual(Buffer.from('Zm9vLQ', 'base64url').toString('utf8'), 'foo-');
assert.strictEqual(Buffer.from('Zm9vLw', 'base64url').toString('utf8'), 'foo/');
assert.strictEqual(Buffer.from('YQ', 'base64url').toString('utf8'), 'a');
assert.strictEqual(Buffer.from('YQ==', 'base64url').toString('utf8'), 'a');

assert.strictEqual(Buffer.byteLength('YQ==', 'base64'), 1);
assert.strictEqual(Buffer.byteLength('YQ==', 'base64url'), 1);
assert.strictEqual(Buffer.byteLength('YWI=', 'base64'), 2);
assert.strictEqual(Buffer.byteLength('YWJj', 'base64'), 3);
assert.strictEqual(Buffer.byteLength('YQ$%', 'base64'), 3);
assert.strictEqual(Buffer.byteLength('Y===', 'base64'), 1);
assert.strictEqual(Buffer.byteLength('aaaa==', 'base64'), 3);
assert.strictEqual(Buffer.byteLength('aaaa==', 'base64url'), 3);

const encodedBase64 = Buffer.from('hello', 'utf8').toString('base64');
assert.strictEqual(encodedBase64, 'aGVsbG8=');
const encodedBase64Url = Buffer.from('hello', 'utf8').toString('base64url');
assert.strictEqual(encodedBase64Url, 'aGVsbG8');
