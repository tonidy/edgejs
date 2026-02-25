'use strict';
const fs = require('fs');
const path = require('path');

const tmpDir = path.join(process.cwd(), 'unode_fs_test_tmp_' + Date.now());
const subDir = path.join(tmpDir, 'a', 'b');
const filePath = path.join(subDir, 'file.txt');
const content = 'hello fs sync';

fs.mkdirSync(subDir, { recursive: true });
const firstCreated = fs.mkdirSync(subDir, { recursive: true });
if (firstCreated !== undefined && firstCreated !== subDir) {
  console.error('mkdirSync recursive return value unexpected:', firstCreated);
  process.exit(1);
}

fs.writeFileSync(filePath, content, { encoding: 'utf8' });
const read = fs.readFileSync(filePath, { encoding: 'utf8' });
if (read !== content) {
  console.error('readFileSync mismatch:', read, '!==', content);
  process.exit(1);
}

const entries = fs.readdirSync(subDir);
if (!Array.isArray(entries) || entries.length !== 1 || entries[0] !== 'file.txt') {
  console.error('readdirSync unexpected:', entries);
  process.exit(1);
}

const withTypes = fs.readdirSync(subDir, { withFileTypes: true });
if (!Array.isArray(withTypes) || withTypes.length !== 1) {
  console.error('readdirSync withFileTypes unexpected:', withTypes);
  process.exit(1);
}
if (withTypes[0].name !== 'file.txt' || !withTypes[0].isFile()) {
  console.error('Dirent unexpected:', withTypes[0]);
  process.exit(1);
}

const resolved = fs.realpathSync(filePath);
if (!resolved || !resolved.includes('file.txt')) {
  console.error('realpathSync unexpected:', resolved);
  process.exit(1);
}

fs.rmSync(tmpDir, { recursive: true });
try {
  fs.readFileSync(filePath);
  console.error('File should not exist after rmSync');
  process.exit(1);
} catch (e) {
  if (e.code !== 'ENOENT') throw e;
}
console.log('fs sync test passed');
