const fs = require('fs');
const path = require('path');
const os = require('os');
const { findPackageJSON } = require('node:module');

console.log('Debugging specific issue...');

// Create the exact scenario from the failing test
const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'jsrt-test-'));
console.log('Created temp dir:', tempDir);

const packagePath = path.join(tempDir, 'package.json');
const packageContent = { name: 'test-package', version: '1.0.0' };
fs.writeFileSync(packagePath, JSON.stringify(packageContent, null, 2));
console.log('Created package.json at:', packagePath);

// Test the failing case
const testFile = './some-file.js';
const result = findPackageJSON(testFile, tempDir);
console.log('findPackageJSON call:');
console.log('  specifier:', testFile);
console.log('  base:', tempDir);
console.log('  result:', result);
console.log('  expected:', packagePath);

// Clean up
fs.rmSync(tempDir, { recursive: true, force: true });
