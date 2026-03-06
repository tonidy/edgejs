const fs = require('fs');
const info = {
  stdout_type: process.stdout?._type,
  stdout_isTTY: process.stdout?.isTTY,
  stdout_ctor: process.stdout?.constructor?.name,
  stdout_handle_ctor: process.stdout?._handle?.constructor?.name,
  stdout_handle_keys: process.stdout?._handle ? Object.getOwnPropertyNames(process.stdout._handle).sort() : null,
  stderr_type: process.stderr?._type,
  stderr_isTTY: process.stderr?.isTTY,
  stderr_ctor: process.stderr?.constructor?.name,
  stderr_handle_ctor: process.stderr?._handle?.constructor?.name,
  stderr_handle_keys: process.stderr?._handle ? Object.getOwnPropertyNames(process.stderr._handle).sort() : null,
};
fs.writeFileSync('/app/stdio-info.json', JSON.stringify(info, null, 2));
