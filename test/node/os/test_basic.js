const assert = require('jsrt:assert');

// Test CommonJS require
const os = require('node:os');

const arch = os.arch();
assert.ok(typeof arch === 'string');
assert.ok(arch.length > 0);

const platform = os.platform();
assert.ok(typeof platform === 'string');
assert.ok(
  [
    'linux',
    'darwin',
    'win32',
    'freebsd',
    'openbsd',
    'netbsd',
    'sunos',
  ].includes(platform)
);

const type = os.type();
assert.ok(typeof type === 'string');
assert.ok(type.length > 0);

const release = os.release();
assert.ok(typeof release === 'string');
assert.ok(release.length > 0);

const hostname = os.hostname();
assert.ok(typeof hostname === 'string');
assert.ok(hostname.length > 0);

const tmpdir = os.tmpdir();
assert.ok(typeof tmpdir === 'string');
assert.ok(tmpdir.length > 0);

const homedir = os.homedir();
assert.ok(typeof homedir === 'string');
assert.ok(homedir.length > 0);

const userInfo = os.userInfo();
assert.ok(typeof userInfo === 'object');
assert.ok(typeof userInfo.username === 'string');
assert.ok(userInfo.username.length > 0);
assert.ok(typeof userInfo.homedir === 'string');

const endianness = os.endianness();
assert.ok(endianness === 'LE' || endianness === 'BE');

const eol = os.EOL;
assert.ok(typeof eol === 'string');
assert.ok(eol === '\r\n' || eol === '\n');

// Test cpus() returns real data
const cpus = os.cpus();
assert.ok(Array.isArray(cpus));
assert.ok(cpus.length > 0);
const cpu = cpus[0];
assert.ok(typeof cpu === 'object');
assert.ok(typeof cpu.model === 'string');
assert.ok(typeof cpu.speed === 'number');
assert.ok(typeof cpu.times === 'object');
assert.ok(typeof cpu.times.user === 'number');
assert.ok(typeof cpu.times.nice === 'number');
assert.ok(typeof cpu.times.sys === 'number');
assert.ok(typeof cpu.times.idle === 'number');
assert.ok(typeof cpu.times.irq === 'number');
