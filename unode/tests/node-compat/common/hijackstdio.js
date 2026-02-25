'use strict';

const stdWrite = {};

function hijackStdWritable(name, listener) {
  const stream = process[name];
  const previous = stdWrite[name] = stream.write;

  stream.writeTimes = 0;
  stream.write = function(data, callback) {
    try {
      listener(data);
    } catch (e) {
      process.nextTick(function() { throw e; });
    }
    previous.call(stream, data, callback);
    stream.writeTimes += 1;
    return true;
  };
}

function restoreWritable(name) {
  process[name].write = stdWrite[name];
  delete process[name].writeTimes;
}

module.exports = {
  hijackStdout: hijackStdWritable.bind(null, 'stdout'),
  hijackStderr: hijackStdWritable.bind(null, 'stderr'),
  restoreStdout: restoreWritable.bind(null, 'stdout'),
  restoreStderr: restoreWritable.bind(null, 'stderr'),
};
