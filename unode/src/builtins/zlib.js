'use strict';

const { PassThrough } = require('stream');

function createGzip() {
  const s = new PassThrough();
  s.readable = true;
  s.writable = true;
  return s;
}

module.exports = {
  createGzip,
};
