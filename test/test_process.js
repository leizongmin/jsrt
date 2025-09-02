// Test ES module import
import process from 'std:process';

console.log('Testing std:process module with ES import:');
console.log('process.argv:', process.argv);
console.log('process.argv0:', process.argv0);
console.log('process.pid:', process.pid);
console.log('process.ppid:', process.ppid);
console.log('process.version:', process.version);
console.log('process.platform:', process.platform);
console.log('process.arch:', process.arch);
console.log('process.uptime():', process.uptime());

// Test CommonJS require
const assert = require("std:assert");
const process2 = require('std:process');
console.log('\nTesting std:process module with CommonJS require:');

// Basic assertions for process object properties
assert.ok(Array.isArray(process2.argv), 'process.argv should be an array');
assert.strictEqual(typeof process2.argv0, 'string', 'process.argv0 should be a string');
assert.strictEqual(typeof process2.pid, 'number', 'process.pid should be a number');
assert.strictEqual(typeof process2.ppid, 'number', 'process.ppid should be a number');
assert.strictEqual(typeof process2.version, 'string', 'process.version should be a string');
assert.strictEqual(typeof process2.platform, 'string', 'process.platform should be a string');
assert.strictEqual(typeof process2.arch, 'string', 'process.arch should be a string');
assert.strictEqual(typeof process2.uptime, 'function', 'process.uptime should be a function');
assert.strictEqual(typeof process2.uptime(), 'number', 'process.uptime() should return a number');

console.log('process.argv:', process2.argv);
console.log('process.argv0:', process2.argv0);
console.log('process.pid:', process2.pid);
console.log('process.ppid:', process2.ppid);
console.log('process.version:', process2.version);
console.log('process.platform:', process2.platform);
console.log('process.arch:', process2.arch);
console.log('process.uptime():', process2.uptime());

// Test uptime changes over time
setTimeout(() => {
  console.log('\nUptime after 100ms:', process.uptime());
}, 100);