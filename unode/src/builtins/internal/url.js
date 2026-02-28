'use strict';

const NativeURL = globalThis.URL;
const NativeURLSearchParams = globalThis.URLSearchParams;
const { Buffer } = require('buffer');
const path = require('path');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_FILE_URL_HOST,
    ERR_INVALID_FILE_URL_PATH,
    ERR_INVALID_URL_SCHEME,
  },
} = require('internal/errors');

const bindingUrl = internalBinding('url');
const isWindows = process.platform === 'win32';
const FORWARD_SLASH = /\//g;

const unsafeProtocol = new Set(['javascript', 'javascript:']);
const hostlessProtocol = new Set(['javascript', 'javascript:']);
const slashedProtocol = new Set(['http', 'http:', 'https', 'https:', 'ftp', 'ftp:', 'file', 'file:', 'ws', 'ws:', 'wss', 'wss:']);

if (typeof NativeURL !== 'function') {
  throw new Error('internal/url requires native URL');
}

function domainToASCII(domain) {
  return typeof bindingUrl.domainToASCII === 'function' ?
    bindingUrl.domainToASCII(`${domain}`) :
    String(domain || '').toLowerCase();
}

function domainToUnicode(domain) {
  return typeof bindingUrl.domainToUnicode === 'function' ?
    bindingUrl.domainToUnicode(`${domain}`) :
    String(domain || '');
}

function isURL(self) {
  if (self instanceof NativeURL) return true;
  return self != null &&
    typeof self === 'object' &&
    self[Symbol.toStringTag] === 'URL';
}

function getPathFromURLPosix(url) {
  if (url.hostname !== '') {
    throw new ERR_INVALID_FILE_URL_HOST(process.platform);
  }
  const pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = pathname.codePointAt(n + 2) | 0x20;
      if (pathname[n + 1] === '2' && third === 102) {
        throw new ERR_INVALID_FILE_URL_PATH('must not include encoded / characters', url.input || url);
      }
    }
  }
  return pathname.includes('%') ? decodeURIComponent(pathname) : pathname;
}

function getPathFromURLWin32(url) {
  const hostname = url.hostname;
  let pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = pathname.codePointAt(n + 2) | 0x20;
      if ((pathname[n + 1] === '2' && third === 102) || (pathname[n + 1] === '5' && third === 99)) {
        throw new ERR_INVALID_FILE_URL_PATH('must not include encoded \\ or / characters', url.input || url);
      }
    }
  }
  pathname = pathname.replace(FORWARD_SLASH, '\\');
  if (pathname.includes('%')) pathname = decodeURIComponent(pathname);

  if (hostname !== '') {
    return `\\\\${domainToUnicode(hostname)}${pathname}`;
  }
  const letter = pathname.codePointAt(1) | 0x20;
  const sep = pathname.charAt(2);
  if (letter < 97 || letter > 122 || sep !== ':') {
    throw new ERR_INVALID_FILE_URL_PATH('must be absolute', url.input || url);
  }
  return pathname.slice(1);
}

function fileURLToPath(input, options = undefined) {
  const windows = options?.windows;
  let href = input;
  if (typeof input === 'string') {
    href = input;
  } else if (input && typeof input === 'object' && typeof input.href === 'string') {
    href = input.href;
  } else {
    throw new ERR_INVALID_ARG_TYPE('path', ['string', 'URL'], input);
  }
  const parsed = typeof bindingUrl.parse === 'function' ? bindingUrl.parse(String(href)) : {};
  const protocol = parsed.protocol;
  if (!String(protocol || '').startsWith('file')) throw new ERR_INVALID_URL_SCHEME('file');
  let inputUrl;
  try {
    inputUrl = new NativeURL(String(href));
  } catch {
    inputUrl = href;
  }
  const url = {
    hostname: parsed.hostname || '',
    pathname: parsed.pathname || '/',
    input: inputUrl,
  };
  return (windows ?? isWindows) ? getPathFromURLWin32(url) : getPathFromURLPosix(url);
}

function fileURLToPathBuffer(input, options = undefined) {
  return Buffer.from(fileURLToPath(input, options), 'utf8');
}

function pathToFileURL(filepath, options = undefined) {
  if (typeof filepath !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('path', 'string', filepath);
  }
  const windows = options?.windows ?? isWindows;
  const isUNC = windows && filepath.startsWith('\\\\');
  let resolved = isUNC ? filepath : (windows ? path.win32.resolve(filepath) : path.posix.resolve(filepath));
  if (isUNC || (windows && resolved.startsWith('\\\\'))) {
    const isExtendedUNC = resolved.startsWith('\\\\?\\UNC\\');
    const prefixLength = isExtendedUNC ? 8 : 2;
    const hostnameEndIndex = resolved.indexOf('\\', prefixLength);
    if (hostnameEndIndex === -1) {
      throw new ERR_INVALID_ARG_VALUE('path', resolved);
    }
    if (hostnameEndIndex === 2) {
      throw new ERR_INVALID_ARG_VALUE('path', resolved);
    }
    const hostname = resolved.slice(prefixLength, hostnameEndIndex);
    const href = typeof bindingUrl.pathToFileURL === 'function' ?
      bindingUrl.pathToFileURL(resolved.slice(hostnameEndIndex), windows, hostname) :
      `file://${hostname}${resolved.slice(hostnameEndIndex).replace(/\\/g, '/')}`;
    return new NativeURL(href);
  }
  const last = filepath.charCodeAt(filepath.length - 1);
  if ((last === 47 || (windows && last === 92)) && resolved[resolved.length - 1] !== path.sep) resolved += '/';
  const href = typeof bindingUrl.pathToFileURL === 'function' ?
    bindingUrl.pathToFileURL(resolved, windows) :
    `file://${resolved.replace(/\\/g, '/')}`;
  return new NativeURL(href);
}

function urlToHttpOptions(urlObj) {
  if (urlObj === null || typeof urlObj !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('url', 'object', urlObj);
  }
  const isRealURL = isURL(urlObj);
  let source = urlObj;
  if (isRealURL &&
      (source.protocol === undefined || source.hostname === undefined) &&
      typeof source.href === 'string') {
    if (typeof bindingUrl.parse === 'function') {
      const parsed = bindingUrl.parse(source.href);
      if (parsed && typeof parsed === 'object' && parsed.protocol !== undefined) {
        source = parsed;
      }
    }
  }
  const {
    hostname,
    pathname,
    port,
    username,
    password,
    search,
  } = source;
  const maybe = (v) => (!isRealURL && v === '' ? undefined : v);
  const protocol = maybe(source.protocol);
  const hash = maybe(source.hash);
  const searchValue = maybe(search);
  const pathnameValue = maybe(pathname);
  const hrefValue = isRealURL ? source.href : undefined;
  return {
    __proto__: null,
    ...urlObj,
    protocol,
    hostname: maybe(hostname && hostname[0] === '[' ? hostname.slice(1, -1) : hostname),
    hash,
    search: searchValue,
    pathname: pathnameValue,
    path: `${pathnameValue || ''}${searchValue || ''}`,
    href: hrefValue,
    port: port !== '' ? Number(port) : undefined,
    auth: username || password ? `${decodeURIComponent(username)}:${decodeURIComponent(password)}` : undefined,
  };
}

module.exports = {
  URL: NativeURL,
  URLSearchParams: NativeURLSearchParams,
  domainToASCII,
  domainToUnicode,
  fileURLToPath,
  fileURLToPathBuffer,
  pathToFileURL,
  isURL,
  urlToHttpOptions,
  unsafeProtocol,
  hostlessProtocol,
  slashedProtocol,
};
