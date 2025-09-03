// Demo showcasing the process module
import process from 'jsrt:process';

console.log('='.repeat(50));
console.log('Process Module Demo');
console.log('='.repeat(50));

console.log('Command line arguments:');
process.argv.forEach((arg, index) => {
  console.log(`  [${index}] ${arg}`);
});
console.log('Executable path (argv0):', process.argv0);

console.log('\nProcess information:');
console.log(`  Process ID (PID): ${process.pid}`);
console.log(`  Parent Process ID (PPID): ${process.ppid}`);
console.log(`  Executable path (argv0): ${process.argv0}`);
console.log(`  Runtime version: ${process.version}`);
console.log(`  Platform: ${process.platform}`);
console.log(`  Architecture: ${process.arch}`);

console.log(`\nUptime at start: ${process.uptime()}`);

// Test CommonJS require as well
const processRequire = require('jsrt:process');
console.log('\nUsing require():');
console.log(`  PID via require: ${processRequire.pid}`);
console.log(`  Platform via require: ${processRequire.platform}`);

// Test uptime measurement
setTimeout(() => {
  console.log(`\nUptime after 50ms delay: ${process.uptime()}`);
}, 50);
