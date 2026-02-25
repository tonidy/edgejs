'use strict';
require('../common');
const {
  hijackStdout,
  hijackStderr,
  restoreStdout,
  restoreStderr
} = require('../common/hijackstdio');

const assert = require('assert');
const Console = require('console').Console;

let c, stdout, stderr;

function setup(groupIndentation) {
  stdout = '';
  hijackStdout(function(data) {
    stdout += data;
  });

  stderr = '';
  hijackStderr(function(data) {
    stderr += data;
  });

  c = new Console({ stdout: process.stdout,
                    stderr: process.stderr,
                    colorMode: false,
                    groupIndentation: groupIndentation });
}

function teardown() {
  restoreStdout();
  restoreStderr();
}

{
  setup();
  const expectedOut = 'This is the outer level\n' +
                      '  Level 2\n' +
                      '    Level 3\n' +
                      '  Back to level 2\n' +
                      'Back to the outer level\n' +
                      'Still at the outer level\n';
  const expectedErr = '    More of level 3\n';

  c.log('This is the outer level');
  c.group();
  c.log('Level 2');
  c.group();
  c.log('Level 3');
  c.warn('More of level 3');
  c.groupEnd();
  c.log('Back to level 2');
  c.groupEnd();
  c.log('Back to the outer level');
  c.groupEnd();
  c.log('Still at the outer level');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

{
  setup();
  const expectedOut = 'No indentation\n' +
                      'None here either\n' +
                      '  Now the first console is indenting\n' +
                      'But the second one does not\n';
  const expectedErr = '';

  const c2 = new Console(process.stdout, process.stderr);
  c.log('No indentation');
  c2.log('None here either');
  c.group();
  c.log('Now the first console is indenting');
  c2.log('But the second one does not');

  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, expectedErr);
  teardown();
}

{
  setup();
  const expectedOut = 'This is a label\n' +
                      '  And this is the data for that label\n';
  c.group('This is a label');
  c.log('And this is the data for that label');
  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, '');
  teardown();
}

{
  setup();
  const expectedOut = 'Label\n' +
                      '  Level 2\n' +
                      '    Level 3\n';
  c.groupCollapsed('Label');
  c.log('Level 2');
  c.groupCollapsed();
  c.log('Level 3');
  assert.strictEqual(stdout, expectedOut);
  assert.strictEqual(stderr, '');
  teardown();
}

{
  setup();
  c.log('not indented');
  c.group();
  c.log('indented\nalso indented');
  c.log({ also: 'a', multiline: 'object' });
  assert.match(stdout, /^not indented\n  indented\n  also indented\n  \{ also: 'a', multiline: 'object' \}\n$/);
  assert.strictEqual(stderr, '');
  teardown();
}

{
  const keys = Reflect.ownKeys(console)
    .filter((val) => Object.prototype.propertyIsEnumerable.call(console, val))
    .map((val) => val.toString());
  assert(!keys.includes('Symbol(groupIndent)'), 'groupIndent should not be enumerable');
}

{
  setup(3);
  c.log('This is the outer level');
  c.group();
  c.log('Level 2');
  assert.strictEqual(stdout, 'This is the outer level\n   Level 2\n');
  teardown();
}

{
  [null, 'str', [], false, true, {}].forEach((e) => {
    assert.throws(
      () => {
        new Console({ stdout: process.stdout,
                      stderr: process.stderr,
                      groupIndentation: e });
      },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  [NaN, 1.01].forEach((e) => {
    assert.throws(
      () => {
        new Console({ stdout: process.stdout,
                      stderr: process.stderr,
                      groupIndentation: e });
      },
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
      }
    );
  });

  [-1, 1001].forEach((e) => {
    assert.throws(
      () => {
        new Console({ stdout: process.stdout,
                      stderr: process.stderr,
                      groupIndentation: e });
      },
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
      }
    );
  });
}
