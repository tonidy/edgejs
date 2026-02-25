'use strict';

const {
  Array,
  Int8Array,
  NumberPrototypeToString,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
} = primordials;

const { ERR_INVALID_URI } = require('internal/errors').codes;

const hexTable = new Array(256);
for (let i = 0; i < 256; ++i)
  hexTable[i] = '%' +
                StringPrototypeToUpperCase((i < 16 ? '0' : '') +
                                           NumberPrototypeToString(i, 16));

const isHexTable = new Int8Array([
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
]);

function encodeStr(str, noEscapeTable, table) {
  const len = str.length;
  if (len === 0) return '';

  let out = '';
  let lastPos = 0;
  let i = 0;

  outer:
  for (; i < len; i++) {
    let c = StringPrototypeCharCodeAt(str, i);
    while (c < 0x80) {
      if (noEscapeTable[c] !== 1) {
        if (lastPos < i) out += StringPrototypeSlice(str, lastPos, i);
        lastPos = i + 1;
        out += table[c];
      }
      if (++i === len) break outer;
      c = StringPrototypeCharCodeAt(str, i);
    }

    if (lastPos < i) out += StringPrototypeSlice(str, lastPos, i);
    if (c < 0x800) {
      lastPos = i + 1;
      out += table[0xC0 | (c >> 6)] + table[0x80 | (c & 0x3F)];
      continue;
    }
    if (c < 0xD800 || c >= 0xE000) {
      lastPos = i + 1;
      out += table[0xE0 | (c >> 12)] +
             table[0x80 | ((c >> 6) & 0x3F)] +
             table[0x80 | (c & 0x3F)];
      continue;
    }
    ++i;
    if (i >= len) throw new ERR_INVALID_URI();
    const c2 = StringPrototypeCharCodeAt(str, i) & 0x3FF;
    lastPos = i + 1;
    c = 0x10000 + (((c & 0x3FF) << 10) | c2);
    out += table[0xF0 | (c >> 18)] +
           table[0x80 | ((c >> 12) & 0x3F)] +
           table[0x80 | ((c >> 6) & 0x3F)] +
           table[0x80 | (c & 0x3F)];
  }

  if (lastPos === 0) return str;
  if (lastPos < len) return out + StringPrototypeSlice(str, lastPos);
  return out;
}

module.exports = {
  encodeStr,
  hexTable,
  isHexTable,
};
