'use strict';

const common = require('../common');
const assert = require('assert');

const stdoutWrite = process.stdout.write;
const check = '\u001b[1;1H\u001b[0J';

function doTest(isTTY, expected) {
  let buf = '';
  process.stdout.isTTY = isTTY;
  process.stdout.write = (string) => { buf += string; return true; };
  console.clear();
  process.stdout.write = stdoutWrite;
  assert.strictEqual(buf, expected);
}

if (!common.isDumbTerminal) {
  doTest(true, check);
}
doTest(false, '');
