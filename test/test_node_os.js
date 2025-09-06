const assert = require('jsrt:assert');

console.log('Testing node:os module functionality...');

// Test CommonJS require
const os = require('node:os');

console.log('Testing os.arch()...');
const arch = os.arch();
console.log('Architecture:', arch);
assert.ok(typeof arch === 'string');
assert.ok(arch.length > 0);

console.log('Testing os.platform()...');
const platform = os.platform();
console.log('Platform:', platform);
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

console.log('Testing os.type()...');
const type = os.type();
console.log('OS Type:', type);
assert.ok(typeof type === 'string');
assert.ok(type.length > 0);

console.log('Testing os.release()...');
const release = os.release();
console.log('OS Release:', release);
assert.ok(typeof release === 'string');
assert.ok(release.length > 0);

console.log('Testing os.hostname()...');
const hostname = os.hostname();
console.log('Hostname:', hostname);
assert.ok(typeof hostname === 'string');
assert.ok(hostname.length > 0);

console.log('Testing os.tmpdir()...');
const tmpdir = os.tmpdir();
console.log('Temp directory:', tmpdir);
assert.ok(typeof tmpdir === 'string');
assert.ok(tmpdir.length > 0);

console.log('Testing os.homedir()...');
const homedir = os.homedir();
console.log('Home directory:', homedir);
assert.ok(typeof homedir === 'string');
assert.ok(homedir.length > 0);

console.log('Testing os.userInfo()...');
const userInfo = os.userInfo();
console.log('User info:', userInfo);
assert.ok(typeof userInfo === 'object');
assert.ok(typeof userInfo.username === 'string');
assert.ok(userInfo.username.length > 0);
assert.ok(typeof userInfo.homedir === 'string');

console.log('Testing os.endianness()...');
const endianness = os.endianness();
console.log('Endianness:', endianness);
assert.ok(endianness === 'LE' || endianness === 'BE');

console.log('Testing os.EOL...');
const eol = os.EOL;
console.log('End of line:', JSON.stringify(eol));
assert.ok(typeof eol === 'string');
assert.ok(eol === '\n' || eol === '\r\n');

console.log('✅ All node:os tests passed');
console.log('=== Node.js OS module implementation complete ===');
