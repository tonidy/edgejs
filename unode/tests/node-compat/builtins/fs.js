'use strict';

const binding = globalThis.__unode_fs;
if (!binding) {
  throw new Error('fs builtin requires __unode_fs binding');
}

function pathTypeError(path) {
  const err = new TypeError('path must be a string, Buffer, or URL. Received ' + (path === null ? 'null' : typeof path));
  err.code = 'ERR_INVALID_ARG_TYPE';
  return err;
}

function getValidatedPath(path) {
  if (path == null) {
    throw pathTypeError(path);
  }
  if (typeof path === 'object' && path && path.href !== undefined) {
    path = path.pathname || path.href;
  } else if (typeof path === 'object' && path) {
    throw pathTypeError(path);
  }
  if (typeof path !== 'string' && typeof path === 'object' && path && path.toString) {
    path = path.toString();
  }
  if (typeof path !== 'string') {
    throw pathTypeError(path);
  }
  if (path.includes('\u0000')) {
    throw new Error('path must be a string without null bytes');
  }
  return path;
}

function getOptions(options, defaults) {
  if (options == null || typeof options === 'function') {
    return Object.assign({}, defaults);
  }
  if (typeof options === 'string') {
    const o = Object.assign({}, defaults);
    o.encoding = options;
    return o;
  }
  if (typeof options !== 'object') {
    throw new Error('options must be an object or string');
  }
  return Object.assign({}, defaults, options);
}

function stringToFlags(flags) {
  if (typeof flags === 'number') {
    return flags;
  }
  if (flags == null) {
    return binding.O_RDONLY;
  }
  switch (flags) {
    case 'r':
      return binding.O_RDONLY;
    case 'rs':
    case 'sr':
      return binding.O_RDONLY | binding.O_SYNC;
    case 'r+':
      return binding.O_RDWR;
    case 'rs+':
    case 'sr+':
      return binding.O_RDWR | binding.O_SYNC;
    case 'w':
      return binding.O_TRUNC | binding.O_CREAT | binding.O_WRONLY;
    case 'wx':
    case 'xw':
      return binding.O_TRUNC | binding.O_CREAT | binding.O_WRONLY | binding.O_EXCL;
    case 'w+':
      return binding.O_TRUNC | binding.O_CREAT | binding.O_RDWR;
    case 'wx+':
    case 'xw+':
      return binding.O_TRUNC | binding.O_CREAT | binding.O_RDWR | binding.O_EXCL;
    case 'a':
      return binding.O_APPEND | binding.O_CREAT | binding.O_WRONLY;
    case 'ax':
    case 'xa':
      return binding.O_APPEND | binding.O_CREAT | binding.O_WRONLY | binding.O_EXCL;
    case 'as':
    case 'sa':
      return binding.O_APPEND | binding.O_CREAT | binding.O_WRONLY | binding.O_SYNC;
    case 'a+':
      return binding.O_APPEND | binding.O_CREAT | binding.O_RDWR;
    case 'ax+':
    case 'xa+':
      return binding.O_APPEND | binding.O_CREAT | binding.O_RDWR | binding.O_EXCL;
    case 'as+':
    case 'sa+':
      return binding.O_APPEND | binding.O_CREAT | binding.O_RDWR | binding.O_SYNC;
    default:
      throw new Error(`Invalid flag: ${flags}`);
  }
}

function parseFileMode(mode, name, def) {
  if (mode === undefined || mode === null) {
    mode = def;
  }
  if (typeof mode === 'string') {
    if (!/^0o[0-7]+$/.test(mode) && !/^[0-7]+$/.test(mode)) {
      throw new Error(`Invalid ${name}: ${mode}`);
    }
    mode = parseInt(mode.replace(/^0o/, ''), 8);
  }
  if (typeof mode !== 'number' || mode < 0 || mode > 0xFFFF) {
    throw new Error(`Invalid ${name}: ${mode}`);
  }
  return mode >>> 0;
}

const defaultRmOptions = {
  recursive: false,
  force: false,
  retryDelay: 100,
  maxRetries: 0,
};

function validateRmOptionsSync(path, options) {
  const opts = Object.assign({}, defaultRmOptions, options && typeof options === 'object' ? options : {});
  if (typeof opts.recursive !== 'boolean') opts.recursive = false;
  if (typeof opts.retryDelay !== 'number' || opts.retryDelay < 0) opts.retryDelay = 100;
  if (typeof opts.maxRetries !== 'number' || opts.maxRetries < 0) opts.maxRetries = 0;
  return { maxRetries: opts.maxRetries, recursive: opts.recursive, retryDelay: opts.retryDelay };
}

class Dirent {
  constructor(name, type, path) {
    this.name = name;
    this.parentPath = path;
    this._type = type;
  }

  isDirectory() {
    return this._type === binding.UV_DIRENT_DIR;
  }

  isFile() {
    return this._type === binding.UV_DIRENT_FILE;
  }

  isBlockDevice() {
    return this._type === binding.UV_DIRENT_BLOCK;
  }

  isCharacterDevice() {
    return this._type === binding.UV_DIRENT_CHAR;
  }

  isSymbolicLink() {
    return this._type === binding.UV_DIRENT_LINK;
  }

  isFIFO() {
    return this._type === binding.UV_DIRENT_FIFO;
  }

  isSocket() {
    return this._type === binding.UV_DIRENT_SOCKET;
  }
}

// Stats: binding returns 18-element array [dev, mode, nlink, uid, gid, rdev, blksize, ino, size, blocks, atime_sec, atime_nsec, mtime_sec, mtime_nsec, ctime_sec, ctime_nsec, birthtime_sec, birthtime_nsec]
const kMsPerSec = 1000;
const kNsecPerMs = 1000000;
function msFromTimeSpec(sec, nsec) {
  return sec * kMsPerSec + nsec / kNsecPerMs;
}

function Stats(dev, mode, nlink, uid, gid, rdev, blksize, ino, size, blocks, atimeMs, mtimeMs, ctimeMs, birthtimeMs) {
  this.dev = dev;
  this.mode = mode;
  this.nlink = nlink;
  this.uid = uid;
  this.gid = gid;
  this.rdev = rdev;
  this.blksize = blksize;
  this.ino = ino;
  this.size = size;
  this.blocks = blocks;
  this.atimeMs = atimeMs;
  this.mtimeMs = mtimeMs;
  this.ctimeMs = ctimeMs;
  this.birthtimeMs = birthtimeMs;
}

Stats.prototype._checkModeProperty = function (property) {
  return (this.mode & binding.S_IFMT) === property;
};
Stats.prototype.isDirectory = function () { return this._checkModeProperty(binding.S_IFDIR); };
Stats.prototype.isFile = function () { return this._checkModeProperty(binding.S_IFREG); };
Stats.prototype.isBlockDevice = function () { return this._checkModeProperty(binding.S_IFBLK); };
Stats.prototype.isCharacterDevice = function () { return this._checkModeProperty(binding.S_IFCHR); };
Stats.prototype.isSymbolicLink = function () { return this._checkModeProperty(binding.S_IFLNK); };
Stats.prototype.isFIFO = function () { return this._checkModeProperty(binding.S_IFIFO); };
Stats.prototype.isSocket = function () { return this._checkModeProperty(binding.S_IFSOCK); };

function dateFromMs(ms) {
  return new Date(Math.round(Number(ms)));
}
function makeTimeGetter(msProp, storeProp) {
  return function () {
    if (Object.prototype.hasOwnProperty.call(this, storeProp)) return this[storeProp];
    return dateFromMs(this[msProp]);
  };
}
function makeTimeSetter(storeProp) {
  return function (v) { this[storeProp] = v; };
}
Object.defineProperty(Stats.prototype, 'atime', {
  enumerable: true,
  get: makeTimeGetter('atimeMs', '_atimeValue'),
  set: makeTimeSetter('_atimeValue')
});
Object.defineProperty(Stats.prototype, 'mtime', {
  enumerable: true,
  get: makeTimeGetter('mtimeMs', '_mtimeValue'),
  set: makeTimeSetter('_mtimeValue')
});
Object.defineProperty(Stats.prototype, 'ctime', {
  enumerable: true,
  get: makeTimeGetter('ctimeMs', '_ctimeValue'),
  set: makeTimeSetter('_ctimeValue')
});
Object.defineProperty(Stats.prototype, 'birthtime', {
  enumerable: true,
  get: makeTimeGetter('birthtimeMs', '_birthtimeValue'),
  set: makeTimeSetter('_birthtimeValue')
});

function getStatsFromBinding(arr) {
  return new Stats(
    arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9],
    msFromTimeSpec(arr[10], arr[11]),
    msFromTimeSpec(arr[12], arr[13]),
    msFromTimeSpec(arr[14], arr[15]),
    msFromTimeSpec(arr[16], arr[17])
  );
}

function statSync(path, options) {
  const opts = getOptions(options, { throwIfNoEntry: true });
  path = getValidatedPath(path);
  try {
    const arr = binding.stat(path);
    return getStatsFromBinding(arr);
  } catch (e) {
    if (opts.throwIfNoEntry === false && e.code === 'ENOENT') return undefined;
    throw e;
  }
}

function lstatSync(path, options) {
  const opts = getOptions(options, { throwIfNoEntry: true });
  path = getValidatedPath(path);
  try {
    const arr = binding.lstat(path);
    return getStatsFromBinding(arr);
  } catch (e) {
    if (opts.throwIfNoEntry === false && e.code === 'ENOENT') return undefined;
    throw e;
  }
}

function fstatSync(fd, options) {
  if (typeof fd !== 'number' || Number.isNaN(fd)) {
    const err = new TypeError('The "fd" argument must be a number. Received ' + (typeof fd === 'symbol' ? 'Symbol' : String(fd)));
    err.code = 'ERR_INVALID_ARG_TYPE';
    throw err;
  }
  const arr = binding.fstat(fd);
  if (arr == null) return undefined;
  return getStatsFromBinding(arr);
}

function existsSync(path) {
  try {
    path = getValidatedPath(path);
  } catch {
    return false;
  }
  return binding.existsSync(path);
}

function exists(path, callback) {
  if (typeof callback !== 'function') {
    const err = new TypeError('Callback must be a function. Received ' + String(callback));
    err.code = 'ERR_INVALID_ARG_TYPE';
    throw err;
  }
  const setImmediate = globalThis.setImmediate || function (fn) { fn(); };
  setImmediate(() => {
    try {
      path = getValidatedPath(path);
      const ok = binding.existsSync(path);
      callback(ok);
    } catch {
      callback(false);
    }
  });
}

function accessSync(path, mode) {
  const m = mode === undefined ? binding.F_OK : mode;
  path = getValidatedPath(path);
  binding.accessSync(path, m);
}

function openSync(path, flags, mode) {
  path = getValidatedPath(path);
  const f = flags == null ? binding.O_RDONLY : (typeof flags === 'string' ? stringToFlags(flags) : flags);
  const m = mode === undefined || mode === null ? 0o666 : parseFileMode(mode, 'mode', 0o666);
  return binding.open(path, f, m);
}

function closeSync(fd) {
  binding.close(fd);
}

const setImmediateOrSync = globalThis.setImmediate || function (fn) { fn(); };

function makeCallback(cb) {
  if (typeof cb !== 'function') return () => {};
  return function (err) {
    if (arguments.length > 1) cb(err, arguments[1]); else cb(err);
  };
}

function stat(path, options, callback) {
  if (typeof options === 'function') { callback = options; options = {}; }
  callback = makeCallback(callback);
  const p = getValidatedPath(path);
  setImmediateOrSync(() => {
    try {
      const arr = binding.stat(p);
      callback(null, getStatsFromBinding(arr));
    } catch (e) {
      callback(e);
    }
  });
}

function lstat(path, options, callback) {
  if (typeof options === 'function') { callback = options; options = {}; }
  callback = makeCallback(callback);
  const p = getValidatedPath(path);
  setImmediateOrSync(() => {
    try {
      const arr = binding.lstat(p);
      callback(null, getStatsFromBinding(arr));
    } catch (e) {
      callback(e);
    }
  });
}

function fstat(fd, options, callback) {
  if (typeof options === 'function') { callback = options; options = {}; }
  if (typeof fd !== 'number') {
    const err = new TypeError('The "fd" argument must be a number. Received ' + String(fd));
    err.code = 'ERR_INVALID_ARG_TYPE';
    throw err;
  }
  callback = makeCallback(callback);
  setImmediateOrSync(() => {
    try {
      const arr = binding.fstat(fd);
      callback(null, getStatsFromBinding(arr));
    } catch (e) {
      callback(e);
    }
  });
}

function open(path, flags, mode, callback) {
  if (arguments.length < 3) { callback = flags; flags = 'r'; mode = 0o666; }
  else if (typeof mode === 'function') { callback = mode; mode = 0o666; }
  callback = makeCallback(callback);
  setImmediateOrSync(() => {
    try {
      const p = getValidatedPath(path);
      const f = flags == null ? binding.O_RDONLY : (typeof flags === 'string' ? stringToFlags(flags) : flags);
      const m = mode === undefined || mode === null ? 0o666 : parseFileMode(mode, 'mode', 0o666);
      const fd = binding.open(p, f, m);
      callback(null, fd);
    } catch (e) {
      callback(e);
    }
  });
}

function close(fd, callback) {
  const cb = typeof callback === 'function' ? callback : () => {};
  setImmediateOrSync(() => {
    try {
      binding.close(fd);
      cb();
    } catch (e) {
      cb(e);
    }
  });
}

function readFileSync(path, options) {
  options = getOptions(options, { flag: 'r', encoding: 'utf8' });
  if (options.encoding === 'utf8' || options.encoding === 'utf-8' || options.encoding === undefined) {
    path = getValidatedPath(path);
    return binding.readFileUtf8(path, stringToFlags(options.flag));
  }
  throw new Error('Only utf8 encoding is supported in this build');
}

function writeFileSync(path, data, options) {
  options = getOptions(options, { encoding: 'utf8', mode: 0o666, flag: 'w' });
  const flag = options.flag || 'w';
  if (typeof data === 'string' && (options.encoding === 'utf8' || options.encoding === 'utf-8')) {
    path = getValidatedPath(path);
    binding.writeFileUtf8(path, data, stringToFlags(flag), parseFileMode(options.mode, 'mode', 0o666));
    return;
  }
  throw new Error('Only string data with utf8 encoding is supported in this build');
}

function mkdirSync(path, options) {
  let mode = 0o777;
  let recursive = false;
  if (typeof options === 'number' || typeof options === 'string') {
    mode = parseFileMode(options, 'mode', 0o777);
  } else if (options && typeof options === 'object') {
    if (options.recursive !== undefined) recursive = Boolean(options.recursive);
    if (options.mode !== undefined) mode = parseFileMode(options.mode, 'options.mode', 0o777);
  }
  path = getValidatedPath(path);
  const result = binding.mkdir(path, mode, recursive);
  if (recursive && result !== undefined) {
    return result;
  }
}

function rmSync(path, options) {
  const opts = validateRmOptionsSync(path, options);
  path = getValidatedPath(path);
  binding.rmSync(path, opts.maxRetries, opts.recursive, opts.retryDelay);
}

function readdirSync(path, options) {
  options = getOptions(options, {});
  path = getValidatedPath(path);
  if (options.recursive) {
    throw new Error('readdirSync with recursive is not supported in this build');
  }
  const withFileTypes = Boolean(options.withFileTypes);
  const result = binding.readdir(path, withFileTypes);
  if (withFileTypes && Array.isArray(result) && result.length === 2) {
    const names = result[0];
    const types = result[1];
    const out = [];
    for (let i = 0; i < names.length; i++) {
      out.push(new Dirent(names[i], types[i], path));
    }
    return out;
  }
  return result;
}

function realpathSync(path, options) {
  path = getValidatedPath(path);
  return binding.realpath(path);
}
realpathSync.native = realpathSync;

const constants = binding;

module.exports = {
  readFileSync,
  writeFileSync,
  mkdirSync,
  rmSync,
  readdirSync,
  realpathSync,
  statSync,
  lstatSync,
  fstatSync,
  stat,
  lstat,
  fstat,
  existsSync,
  exists,
  accessSync,
  openSync,
  closeSync,
  open,
  close,
  constants,
  Dirent,
  Stats,
};
