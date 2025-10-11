// Real-world example: Simple file information script
// This demonstrates how Node.js code can run in jsrt with --compact-node flag

const fs = require('fs');
const path = require('path');
const os = require('os');

console.log('=== System Information ===');
console.log('Platform:', os.platform());
console.log('Architecture:', os.arch());
console.log('Hostname:', os.hostname());
console.log('Home directory:', os.homedir());
console.log('Temp directory:', os.tmpdir());
console.log('');

console.log('=== Process Information ===');
console.log('Process ID:', process.pid);
console.log('Node version:', process.version);
console.log('Current directory:', process.cwd());
console.log('');

console.log('=== Path Operations ===');
const testPath = path.join(os.homedir(), 'documents', 'test.txt');
console.log('Joined path:', testPath);
console.log('Directory name:', path.dirname(testPath));
console.log('Base name:', path.basename(testPath));
console.log('Extension:', path.extname(testPath));
console.log('');

console.log('=== File System Operations ===');
try {
  const currentDir = process.cwd();
  console.log('Current directory contents:');
  const files = fs.readdirSync(currentDir);
  files.slice(0, 10).forEach((file) => {
    const stats = fs.statSync(path.join(currentDir, file));
    const type = stats.isDirectory() ? '[DIR]' : '[FILE]';
    console.log(`  ${type} ${file}`);
  });
  if (files.length > 10) {
    console.log(`  ... and ${files.length - 10} more items`);
  }
} catch (error) {
  console.log('Error reading directory:', error.message);
}

console.log('\n✅ Example completed successfully!');
console.log(
  'This script ran with Node.js-compatible code using --compact-node flag'
);
