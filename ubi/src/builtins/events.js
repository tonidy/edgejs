'use strict';

// Ensure Event/EventTarget/CustomEvent globals exist before loading events tests.
require('internal/event_target');

const exported = require('../../../node-lib/events.js');

// Node marks AbortSignal max listeners as 0 by default.
if (typeof AbortSignal === 'function' &&
    AbortSignal.prototype &&
    exported &&
    exported.kMaxEventTargetListeners) {
  try {
    AbortSignal.prototype[exported.kMaxEventTargetListeners] = 0;
  } catch {
    // Ignore if prototype is not writable in this runtime.
  }
}
if (typeof EventTarget === 'function' &&
    EventTarget.prototype &&
    exported &&
    exported.kMaxEventTargetListeners) {
  try {
    if (typeof EventTarget.prototype[exported.kMaxEventTargetListeners] !== 'number') {
      EventTarget.prototype[exported.kMaxEventTargetListeners] = exported.defaultMaxListeners;
    }
  } catch {}
}
if (typeof EventTarget === 'function' &&
    EventTarget.prototype &&
    exported &&
    exported.kMaxEventTargetListenersWarned) {
  try {
    if (typeof EventTarget.prototype[exported.kMaxEventTargetListenersWarned] !== 'boolean') {
      EventTarget.prototype[exported.kMaxEventTargetListenersWarned] = false;
    }
  } catch {}
}

const originalGetMaxListeners = exported.getMaxListeners;
exported.getMaxListeners = function getMaxListenersPatched(emitterOrTarget) {
  if (emitterOrTarget &&
      typeof emitterOrTarget === 'object' &&
      typeof emitterOrTarget.aborted === 'boolean') {
    return 0;
  }
  return originalGetMaxListeners(emitterOrTarget);
};

module.exports = exported;
