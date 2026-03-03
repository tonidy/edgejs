'use strict';

const kUvErrMap = new Map([
  [-2, ['ENOENT', 'no such file or directory']],
  [-9, ['EBADF', 'bad file descriptor']],
  [-12, ['ENOMEM', 'out of memory']],
  [-13, ['EACCES', 'permission denied']],
  [-22, ['EINVAL', 'invalid argument']],
  [-38, ['ENOSYS', 'function not implemented']],
  [-55, ['ENOBUFS', 'no buffer space available']],
  [-60, ['ETIMEDOUT', 'connection timed out']],
  [-105, ['ENOBUFS', 'no buffer space available']],
  [-110, ['ETIMEDOUT', 'connection timed out']],
  [-3007, ['ENOTFOUND', 'name does not resolve']],
  [-3008, ['ENOTFOUND', 'name does not resolve']],
]);

function getUvErrorEntry(err) {
  const n = Number(err);
  if (!Number.isFinite(n)) return undefined;
  if (kUvErrMap.has(n)) return kUvErrMap.get(n);

  let name;
  try {
    const errno = require('os').constants && require('os').constants.errno;
    if (errno && typeof errno === 'object') {
      for (const key of Object.keys(errno)) {
        if (-Number(errno[key]) === n) {
          name = key;
          break;
        }
      }
    }
  } catch {}
  if (!name) return undefined;

  const entry = [name, name];
  kUvErrMap.set(n, entry);
  return entry;
}

function getUvErrorMap() {
  return kUvErrMap;
}

function getUvErrorMessage(err) {
  const row = getUvErrorEntry(err);
  return row ? String(row[1]) : `Unknown system error ${String(err)}`;
}

module.exports = {
  getUvErrorEntry,
  getUvErrorMap,
  getUvErrorMessage,
};
