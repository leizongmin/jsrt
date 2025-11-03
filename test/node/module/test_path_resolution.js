const fs = require('fs');
const path = require('path');
const os = require('os');

console.log('Testing path resolution manually...');

// Test the exact same path resolution that our C code is doing
const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'jsrt-test-'));
console.log('Temp dir:', tempDir);

// Create package.json first
const packageContent = { name: 'test-package', version: '1.0.0' };
const packageJsonPath = path.join(tempDir, 'package.json');
fs.writeFileSync(packageJsonPath, JSON.stringify(packageContent, null, 2));
console.log('Created package.json at:', packageJsonPath);

const specifier = './some-file.js';
const base = tempDir;

// Simulate what our C code does
let resolved_path;
if (path.isAbsolute(specifier)) {
  resolved_path = specifier;
} else if (specifier.startsWith('./') || specifier.startsWith('../')) {
  resolved_path = path.resolve(base, specifier);
} else {
  resolved_path = path.join(base, specifier);
}

console.log('Specifier:', specifier);
console.log('Base:', base);
console.log('Resolved path:', resolved_path);

// Check if it looks like a file
const last_sep = resolved_path.lastIndexOf(path.sep);
const last_dot = resolved_path.lastIndexOf('.');
const looks_like_file = last_sep > -1 && last_dot > last_sep;
console.log('Looks like file:', looks_like_file);

// Get parent directory
const search_dir = looks_like_file
  ? path.dirname(resolved_path)
  : resolved_path;
console.log('Search directory:', search_dir);

// Check if package.json exists in search_dir
const package_json_path = path.join(search_dir, 'package.json');
const exists = fs.existsSync(package_json_path);
console.log('Package.json exists:', exists);
console.log('Package.json path:', package_json_path);

// Clean up
fs.rmSync(tempDir, { recursive: true, force: true });
