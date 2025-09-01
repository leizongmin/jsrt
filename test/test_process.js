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
const process2 = require('std:process');
console.log('\nTesting std:process module with CommonJS require:');
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