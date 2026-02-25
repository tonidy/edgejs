'use strict';

const binding = globalThis.__unode_fs;
if (!binding) {
  throw new Error('fs builtin requires __unode_fs binding');
}

function getValidatedPath(path) {
  if (path == null) {
    throw new Error('path must be a string, Buffer, or URL');
  }
  if (typeof path === 'object' && path && path.href !== undefined) {
    path = path.pathname || path.href;
  }
  if (typeof path !== 'string' && typeof path === 'object' && path && path.toString) {
    path = path.toString();
  }
  if (typeof path !== 'string') {
    throw new Error('path must be a string, Buffer, or URL');
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
  constants,
  Dirent,
};
