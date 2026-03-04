'use strict';

function getBuiltinModule(id) {
  if (typeof id !== 'string') return undefined;
  const specifier = id.startsWith('node:') ? id.slice(5) : id;
  try {
    return require(specifier);
  } catch {
    return undefined;
  }
}

module.exports = {
  getBuiltinModule,
};
