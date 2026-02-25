'use strict';

function join() {
  const parts = [];
  for (let i = 0; i < arguments.length; i += 1) {
    const p = String(arguments[i]).replace(/\/+$/, '');
    if (p) parts.push(p);
  }
  return parts.join('/').replace(/\/+/g, '/');
}

module.exports = { join };
