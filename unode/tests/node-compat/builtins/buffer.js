'use strict';

const BufferObj = globalThis.Buffer || {};
const encodingBinding = globalThis.__unode_encoding || null;

function isUint8Array(value) {
  return value && typeof value === 'object' && value.buffer instanceof ArrayBuffer &&
    typeof value.byteLength === 'number' && typeof value.length === 'number';
}

function attachBufferLikeMethods(view) {
  if (!isUint8Array(view)) return view;
  if (!view.equals) {
    Object.defineProperty(view, 'equals', {
      configurable: true,
      value(other) {
        if (!isUint8Array(other) || this.length !== other.length) return false;
        for (let i = 0; i < this.length; i++) {
          if (this[i] !== other[i]) return false;
        }
        return true;
      },
    });
  }
  Object.defineProperty(view, 'toString', {
    configurable: true,
    value(enc) {
      const encoding = enc == null ? 'utf8' : String(enc).toLowerCase();
      if (encodingBinding && typeof encodingBinding.encodeBase64 === 'function') {
        if (encoding === 'base64') return encodingBinding.encodeBase64(this, false);
        if (encoding === 'base64url') return encodingBinding.encodeBase64(this, true).replace(/=+$/g, '');
      }
      if (encodingBinding && typeof encodingBinding.decodeUtf8 === 'function') {
        if (encoding === 'utf8' || encoding === 'utf-8') return encodingBinding.decodeUtf8(this);
      }
      let out = '';
      for (let i = 0; i < this.length; i++) out += String.fromCharCode(this[i]);
      return out;
    },
  });
  return view;
}

function base64ByteLength(str) {
  let bytes = str.length;
  if (bytes > 0 && str.charCodeAt(bytes - 1) === 0x3D) bytes--;
  if (bytes > 1 && str.charCodeAt(bytes - 1) === 0x3D) bytes--;
  return (bytes * 3) >>> 2;
}

if (encodingBinding && typeof encodingBinding.encodeUtf8 === 'function') {
  const originalFrom = BufferObj.from;
  BufferObj.from = function from(value, encoding) {
    if (typeof value === 'string') {
      const enc = encoding == null ? 'utf8' : String(encoding).toLowerCase();
      if (enc === 'utf8' || enc === 'utf-8') {
        return attachBufferLikeMethods(encodingBinding.encodeUtf8(value));
      }
      if ((enc === 'base64' || enc === 'base64url') && typeof encodingBinding.decodeBase64 === 'function') {
        return attachBufferLikeMethods(encodingBinding.decodeBase64(value, enc === 'base64url'));
      }
    }
    if (typeof originalFrom === 'function') {
      return attachBufferLikeMethods(originalFrom.call(this, value, encoding));
    }
    if (value && typeof value.length === 'number') {
      return attachBufferLikeMethods(new Uint8Array(value));
    }
    return attachBufferLikeMethods(new Uint8Array(0));
  };
}

if (encodingBinding && typeof encodingBinding.utf8ByteLength === 'function') {
  BufferObj.byteLength = function byteLength(value, encoding) {
    if (typeof value === 'string') {
      const enc = encoding == null ? 'utf8' : String(encoding).toLowerCase();
      if (enc === 'utf8' || enc === 'utf-8') return encodingBinding.utf8ByteLength(value);
      if (enc === 'base64' || enc === 'base64url') {
        return base64ByteLength(value);
      }
    }
    if (typeof value === 'string') return value.length;
    if (value && value.byteLength !== undefined) return value.byteLength;
    if (value && value.length !== undefined) return value.length;
    return 0;
  };
}

const originalAlloc = BufferObj.alloc;
if (typeof originalAlloc === 'function') {
  BufferObj.alloc = function alloc(size) {
    return attachBufferLikeMethods(originalAlloc.call(this, size));
  };
}
const originalAllocUnsafe = BufferObj.allocUnsafe;
if (typeof originalAllocUnsafe === 'function') {
  BufferObj.allocUnsafe = function allocUnsafe(size) {
    return attachBufferLikeMethods(originalAllocUnsafe.call(this, size));
  };
}

module.exports = { Buffer: BufferObj };
