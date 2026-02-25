'use strict';

const primordials = require('internal/util/primordials');
globalThis.primordials = Object.assign({}, globalThis.primordials || {}, primordials);

const path = require('path');
module.exports = require(path.resolve(__dirname, '../../../node/lib/querystring.js'));
