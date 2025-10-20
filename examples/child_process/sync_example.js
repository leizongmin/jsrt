#!/usr/bin/env jsrt
// Synchronous child_process examples

const { execSync, spawnSync } = require('node:child_process');

console.log('=== Synchronous Child Process Examples ===\n');

// Example 1: execSync - simple command
console.log('1. execSync() - simple command:');
try {
  const output = execSync('echo "Hello from execSync!"');
  console.log(`   Output: ${output.toString().trim()}`);
} catch (error) {
  console.error(`   Error: ${error.message}`);
}

// Example 2: execSync - with encoding option
console.log('\n2. execSync() - with encoding:');
try {
  const output = execSync('date', { encoding: 'utf8' });
  console.log(`   Current date: ${output.trim()}`);
} catch (error) {
  console.error(`   Error: ${error.message}`);
}

// Example 3: spawnSync - with arguments
console.log('\n3. spawnSync() - with arguments:');
const result = spawnSync('echo', ['arg1', 'arg2', 'arg3']);
console.log(`   Exit code: ${result.status}`);
console.log(`   Output: ${result.stdout.toString().trim()}`);

// Example 4: spawnSync - capture both stdout and stderr
console.log('\n4. spawnSync() - stdout and stderr:');
const result2 = spawnSync('sh', [
  '-c',
  'echo "stdout message"; echo "stderr message" >&2',
]);
console.log(`   Stdout: ${result2.stdout.toString().trim()}`);
console.log(`   Stderr: ${result2.stderr.toString().trim()}`);

// Example 5: Error handling with execSync
console.log('\n5. Error handling:');
try {
  execSync('nonexistent_command');
} catch (error) {
  console.log(`   ✓ Caught error: ${error.message}`);
  console.log(`   Exit code: ${error.status}`);
}

// Example 6: Timeout handling
console.log('\n6. Timeout handling:');
try {
  execSync('sleep 10', { timeout: 100 });
} catch (error) {
  console.log(`   ✓ Caught timeout: ${error.message}`);
  console.log(`   Signal: ${error.signal}`);
}

console.log('\n✓ All synchronous examples completed');
