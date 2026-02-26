'use strict';

const REPL = require('repl');

module.exports = { __proto__: REPL };
module.exports.createInternalRepl = createRepl;

function createRepl(env, opts, cb) {
  if (typeof opts === 'function') {
    cb = opts;
    opts = null;
  }
  const options = {
    ignoreUndefined: false,
    useGlobal: true,
    breakEvalOnSigint: true,
    ...(opts && typeof opts === 'object' ? opts : {}),
  };

  if (env && Number.parseInt(env.NODE_NO_READLINE, 10)) {
    options.terminal = false;
  }
  if (options.replMode === undefined) {
    options.replMode = REPL.REPL_MODE_SLOPPY;
  }

  const repl = REPL.start(options);
  if (typeof repl.setupHistory === 'function') {
    repl.setupHistory({ historyPath: env && env.NODE_REPL_HISTORY }, (err) => {
      if (typeof cb === 'function') cb(err || null, repl);
    });
    return;
  }
  if (typeof cb === 'function') cb(null, repl);
}
