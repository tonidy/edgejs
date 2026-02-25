'use strict';

const inspectCustom = Symbol('nodejs.util.inspect.custom');

function quoteString(str) {
  return "'" + String(str).replace(/\\/g, '\\\\').replace(/'/g, "\\'") + "'";
}

function formatValue(value, opts, depth) {
  if (value === null) return 'null';
  if (value === undefined) return 'undefined';
  if (typeof value === 'string') return quoteString(value);
  if (typeof value === 'number' || typeof value === 'boolean' || typeof value === 'bigint') return String(value);
  if (typeof value === 'symbol') return value.toString();
  if (typeof value === 'function') return '[Function' + (value.name ? ': ' + value.name : '') + ']';
  if (value && typeof value === 'object' && opts.customInspect !== false && typeof value[inspectCustom] === 'function') {
    return String(value[inspectCustom]());
  }
  if (Array.isArray(value)) {
    if (opts.depth !== undefined && depth >= opts.depth) return '[Array]';
    return '[ ' + value.map(function (v) { return formatValue(v, opts, depth + 1); }).join(', ') + ' ]';
  }
  if (typeof value === 'object') {
    if (opts.depth !== undefined && depth >= opts.depth) return '[Object]';
    const keys = Object.keys(value);
    if (keys.length === 0) return '{}';
    const parts = keys.map(function(k) {
      return k + ': ' + formatValue(value[k], opts, depth + 1);
    });
    return '{ ' + parts.join(', ') + ' }';
  }
  return String(value);
}

function inspect(value, options) {
  const opts = options && typeof options === 'object' ? options : {};
  return formatValue(value, opts, 0);
}

function format(fmt) {
  const args = Array.prototype.slice.call(arguments, 1);
  if (typeof fmt !== 'string') {
    return [fmt].concat(args).map(function(a) { return inspect(a); }).join(' ');
  }
  let idx = 0;
  const out = fmt.replace(/%[sdijfoO%]/g, function(token) {
    if (token === '%%') return '%';
    if (idx >= args.length) return token;
    const val = args[idx++];
    switch (token) {
      case '%s': return String(val);
      case '%d':
      case '%i': return Number.parseInt(val, 10).toString();
      case '%f': return Number(val).toString();
      case '%j':
        try { return JSON.stringify(val); } catch (_) { return '[Circular]'; }
      case '%o':
      case '%O': return inspect(val);
      default: return String(val);
    }
  });
  if (idx < args.length) {
    return out + ' ' + args.slice(idx).map(function(a) {
      if (a === null || a === undefined) return String(a);
      if (typeof a === 'string' || typeof a === 'number' || typeof a === 'boolean' || typeof a === 'bigint' ||
          typeof a === 'symbol') {
        return String(a);
      }
      return inspect(a);
    }).join(' ');
  }
  return out;
}

module.exports = {
  inspect,
  format,
};
module.exports.inspect.custom = inspectCustom;
