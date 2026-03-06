const fs = require('fs');

process.stderr.write('stderr: process.stderr.write\n');
console.error('stderr: console.error');
fs.writeSync(2, 'stderr: fs.writeSync(2, ...)\n');
