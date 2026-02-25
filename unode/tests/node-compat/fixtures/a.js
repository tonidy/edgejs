const c = require('./b/c');

var string = 'A';

exports.SomeClass = c.SomeClass;

exports.A = function() {
  return string;
};

exports.C = function() {
  return c.C();
};

exports.D = function() {
  return c.D();
};

exports.number = 42;
