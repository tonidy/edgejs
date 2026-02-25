'use strict';
const common = require('../common');
const assert = require('assert');
const Stream = require('stream');
const requiredConsole = require('console');
const Console = requiredConsole.Console;

const out = new Stream();
const err = new Stream();

process.stdout.write = process.stderr.write = common.mustNotCall();

assert.strictEqual(typeof Console, 'function');
assert.strictEqual(requiredConsole, global.console);
assert.ok(global.console instanceof Console);
assert.ok(!({} instanceof Console));

assert.throws(
  () => { new Console(); },
  {
    code: 'ERR_CONSOLE_WRITABLE_STREAM',
    name: 'TypeError',
  }
);

assert.throws(
  () => {
    out.write = () => {};
    err.write = undefined;
    new Console(out, err);
  },
  {
    code: 'ERR_CONSOLE_WRITABLE_STREAM',
    name: 'TypeError',
  }
);

out.write = err.write = (d) => {};

{
  const c = new Console(out, err);
  assert.ok(c instanceof Console);

  out.write = err.write = common.mustCall((d) => {
    assert.strictEqual(d, 'test\n');
  }, 2);

  c.log('test');
  c.error('test');

  out.write = common.mustCall((d) => {
    assert.strictEqual(d, "{ foo: 1 }\n");
  });

  c.dir({ foo: 1 });

  let called = 0;
  out.write = common.mustCall((d) => {
    called++;
    assert.strictEqual(d, `${called} ${called - 1} [ 1, 2, 3 ]\n`);
  }, 3);

  [1, 2, 3].forEach(c.log);
}

{
  const withoutNew = Console(out, err);
  assert.ok(withoutNew instanceof Console);
}

{
  class MyConsole extends Console {
    hello() {}
    log() { return 'overridden'; }
  }
  const myConsole = new MyConsole(process.stdout);
  assert.strictEqual(typeof myConsole.hello, 'function');
  assert.ok(myConsole instanceof Console);
  assert.strictEqual(myConsole.log(), 'overridden');

  const log = myConsole.log;
  assert.strictEqual(log(), 'overridden');
}

{
  const c2 = new Console(out, err, false);

  out.write = () => { throw new Error('out'); };
  err.write = () => { throw new Error('err'); };

  assert.throws(() => c2.log('foo'), /^Error: out$/);
  assert.throws(() => c2.warn('foo'), /^Error: err$/);
  assert.throws(() => c2.dir('foo'), /^Error: out$/);
}

[null, true, false, 'foo', 5, Symbol()].forEach((inspectOptions) => {
  assert.throws(
    () => {
      new Console({
        stdout: out,
        stderr: err,
        inspectOptions
      });
    },
    {
      code: 'ERR_INVALID_ARG_TYPE'
    }
  );
});
