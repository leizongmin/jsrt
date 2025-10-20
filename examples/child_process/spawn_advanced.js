#!/usr/bin/env jsrt
// Advanced spawn example - with options and environment

const { spawn } = require('node:child_process');

console.log('=== Advanced spawn() Example ===\n');

// Example 1: Spawn with custom environment
console.log('1. Custom environment variables:');
const envChild = spawn('sh', ['-c', 'echo "MY_VAR=$MY_VAR"'], {
  env: { MY_VAR: 'custom_value', PATH: process.env.PATH },
});

envChild.stdout.on('data', (data) => {
  console.log(`   ${data.toString().trim()}`);
});

// Example 2: Spawn with working directory
setTimeout(() => {
  console.log('\n2. Custom working directory:');
  const cwdChild = spawn('pwd', [], {
    cwd: '/tmp',
  });

  cwdChild.stdout.on('data', (data) => {
    console.log(`   Working directory: ${data.toString().trim()}`);
  });

  cwdChild.on('error', (err) => {
    console.error(`   Error: ${err.message}`);
  });
}, 500);

// Example 3: Detached process
setTimeout(() => {
  console.log('\n3. Detached process:');
  const detached = spawn('sleep', ['1'], {
    detached: true,
    stdio: 'ignore',
  });

  console.log(`   Spawned detached process with PID: ${detached.pid}`);
  console.log('   Parent can exit without waiting for child');

  detached.unref(); // Allow parent to exit
}, 1000);
