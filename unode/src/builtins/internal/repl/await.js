'use strict';

const path = require('path');

function resolveNodeRoot() {
  if (process && process.env && process.env.NODE_TEST_DIR) {
    return path.resolve(process.env.NODE_TEST_DIR, '..');
  }
  const candidates = [
    path.resolve(process.cwd(), '..', 'node'),
    path.resolve(process.cwd(), 'node'),
  ];
  for (const c of candidates) {
    try {
      require.resolve(path.join(c, 'deps/acorn/acorn/dist/acorn'));
      return c;
    } catch {}
  }
  return path.resolve(process.cwd(), '..', 'node');
}

const nodeRoot = resolveNodeRoot();
const parser = require(path.join(nodeRoot, 'deps/acorn/acorn/dist/acorn')).Parser;
const walk = require(path.join(nodeRoot, 'deps/acorn/acorn-walk/dist/walk'));
const { Recoverable } = require('internal/repl');

function isTopLevelDeclaration(state) {
  return state.ancestors[state.ancestors.length - 2] === state.body;
}

const noop = Function.prototype;
const visitorsWithoutAncestors = {
  ClassDeclaration(node, state, c) {
    if (isTopLevelDeclaration(state)) {
      state.prepend(node, `${node.id.name}=`);
      state.hoistedDeclarationStatements.push(`let ${node.id.name}; `);
    }

    walk.base.ClassDeclaration(node, state, c);
  },
  ForOfStatement(node, state, c) {
    if (node.await === true) {
      state.containsAwait = true;
    }
    walk.base.ForOfStatement(node, state, c);
  },
  FunctionDeclaration(node, state, c) {
    state.prepend(node, `this.${node.id.name} = ${node.id.name}; `);
    state.hoistedDeclarationStatements.push(`var ${node.id.name}; `);
  },
  FunctionExpression: noop,
  ArrowFunctionExpression: noop,
  MethodDefinition: noop,
  AwaitExpression(node, state, c) {
    state.containsAwait = true;
    walk.base.AwaitExpression(node, state, c);
  },
  ReturnStatement(node, state, c) {
    state.containsReturn = true;
    walk.base.ReturnStatement(node, state, c);
  },
  VariableDeclaration(node, state, c) {
    const variableKind = node.kind;
    const isIterableForDeclaration = ['ForOfStatement', 'ForInStatement'].includes(
      state.ancestors[state.ancestors.length - 2].type
    );

    if (variableKind === 'var' || isTopLevelDeclaration(state)) {
      state.replace(
        node.start,
        node.start + variableKind.length + (isIterableForDeclaration ? 1 : 0),
        variableKind === 'var' && isIterableForDeclaration ?
          '' :
          'void' + (node.declarations.length === 1 ? '' : ' ('),
      );

      if (!isIterableForDeclaration) {
        node.declarations.forEach((decl) => {
          state.prepend(decl, '(');
          state.append(decl, decl.init ? ')' : '=undefined)');
        });

        if (node.declarations.length !== 1) {
          state.append(node.declarations[node.declarations.length - 1], ')');
        }
      }

      const variableIdentifiersToHoist = [
        ['var', []],
        ['let', []],
      ];
      function registerVariableDeclarationIdentifiers(idNode) {
        if (!idNode) return;
        switch (idNode.type) {
          case 'Identifier':
            variableIdentifiersToHoist[variableKind === 'var' ? 0 : 1][1].push(idNode.name);
            break;
          case 'ObjectPattern':
            idNode.properties.forEach((property) => {
              registerVariableDeclarationIdentifiers(property.value || property.argument);
            });
            break;
          case 'ArrayPattern':
            idNode.elements.forEach((element) => {
              registerVariableDeclarationIdentifiers(element);
            });
            break;
        }
      }

      node.declarations.forEach((decl) => {
        registerVariableDeclarationIdentifiers(decl.id);
      });

      variableIdentifiersToHoist.forEach(([kind, identifiers]) => {
        if (identifiers.length > 0) {
          state.hoistedDeclarationStatements.push(`${kind} ${identifiers.join(', ')}; `);
        }
      });
    }

    walk.base.VariableDeclaration(node, state, c);
  },
};

const visitors = {};
for (const nodeType of Object.keys(walk.base)) {
  const callback = visitorsWithoutAncestors[nodeType] || walk.base[nodeType];
  visitors[nodeType] = (node, state, c) => {
    const isNew = node !== state.ancestors[state.ancestors.length - 1];
    if (isNew) state.ancestors.push(node);
    callback(node, state, c);
    if (isNew) state.ancestors.pop();
  };
}

function processTopLevelAwait(src) {
  const wrapPrefix = '(async () => { ';
  const wrapped = `${wrapPrefix}${src} })()`;
  const wrappedArray = wrapped.split('');
  let root;
  try {
    root = parser.parse(wrapped, { ecmaVersion: 'latest' });
  } catch (e) {
    if (String(e.message).startsWith('Unterminated ')) throw new Recoverable(e);
    const awaitPos = src.indexOf('await');
    const errPos = e.pos - wrapPrefix.length;
    if (awaitPos > errPos) return null;
    if (errPos === awaitPos + 6 && e.message.includes('Expecting Unicode escape sequence')) return null;
    if (errPos === awaitPos + 7 && e.message.includes('Unexpected token')) return null;
    const line = e.loc.line;
    const column = line === 1 ? e.loc.column - wrapPrefix.length : e.loc.column;
    let message = '\n' + src.split('\n', line)[line - 1] + '\n' +
      ' '.repeat(column) + '^\n\n' + e.message.replace(/ \([^)]+\)/, '');
    if (message.endsWith('Unexpected token')) {
      message += ` '${src[e.pos - wrapPrefix.length] ?? src[src.length - 1]}'`;
    }
    throw new SyntaxError(message);
  }
  const body = root.body[0].expression.callee.body;
  const state = {
    body,
    ancestors: [],
    hoistedDeclarationStatements: [],
    replace(from, to, str) {
      for (let i = from; i < to; i++) wrappedArray[i] = '';
      if (from === to) str += wrappedArray[from];
      wrappedArray[from] = str;
    },
    prepend(node, str) {
      wrappedArray[node.start] = str + wrappedArray[node.start];
    },
    append(node, str) {
      wrappedArray[node.end - 1] += str;
    },
    containsAwait: false,
    containsReturn: false,
  };

  walk.recursive(body, state, visitors);

  if (!state.containsAwait || state.containsReturn) return null;

  for (let i = body.body.length - 1; i >= 0; i--) {
    const node = body.body[i];
    if (node.type === 'EmptyStatement') continue;
    if (node.type === 'ExpressionStatement') {
      state.prepend(node.expression, '{ value: (');
      state.prepend(node, 'return ');
      state.append(node.expression, ') }');
    }
    break;
  }

  return state.hoistedDeclarationStatements.join('') + wrappedArray.join('');
}

module.exports = {
  processTopLevelAwait,
};
