'use strict';

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

async function runScript(filePath) {
  return new Promise((resolve) => {
    const child = spawn(process.execPath, [filePath], {
      stdio: 'inherit',
      cwd: __dirname,
      env: process.env,
    });
    const tid = setTimeout(() => child.kill(), 1000);
    child.on('exit', (code, signal) => {
      clearTimeout(tid);
      resolve({ file: path.basename(filePath), code, signal });
    });
    child.on('error', (error) => {
      console.error(`Failed to start ${filePath}:`, error.message);
      clearTimeout(tid);
      resolve({ file: path.basename(filePath), code: 1, signal: null });
    });
  });
}

async function main() {
  const files = fs
    .readdirSync(__dirname)
    .filter((name) => /^test_.*\.js$/i.test(name))
    .sort();

  if (files.length === 0) {
    console.log('No test_*.js files found.');
    return;
  }

  let successes = 0;
  let failures = 0;

  for (const file of files) {
    console.log(`\n=== Running ${file} ===`);
    const result = await runScript(path.join(__dirname, file));

    if (result.code === 0) {
      successes += 1;
      console.log(`--- ${file} PASSED ---`);
    } else {
      failures += 1;
      const reason = result.signal
        ? `signal ${result.signal}`
        : `exit code ${result.code}`;
      console.log(`--- ${file} FAILED (${reason}) ---`);
    }
  }

  console.log('\n================================');
  console.log(`Total files: ${files.length}`);
  console.log(`Passed: ${successes}`);
  console.log(`Failed: ${failures}`);
  console.log('================================\n');

  if (failures > 0) {
    process.exitCode = 1;
  }
}

main().catch((error) => {
  console.error('runall failed:', error);
  process.exitCode = 1;
});
