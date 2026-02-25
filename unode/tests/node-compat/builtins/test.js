'use strict';

function test(_name, fn) {
  if (typeof fn === 'function') fn();
}

function describe(_name, fn) {
  if (typeof fn === 'function') fn();
}

function it(_name, fn) {
  if (typeof fn === 'function') fn();
}

module.exports = {
  test,
  describe,
  it,
};
