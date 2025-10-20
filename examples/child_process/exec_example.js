#!/usr/bin/env jsrt
// exec() example - running shell commands with buffered output

const { exec } = require('node:child_process');

console.log('=== exec() Example ===\n');

// Example 1: Simple shell command
console.log('1. Simple command with callback:');
exec('echo "Hello from exec!"', (error, stdout, stderr) => {
  if (error) {
    console.error(`   Error: ${error.message}`);
    return;
  }
  console.log(`   Output: ${stdout.trim()}`);
});

// Example 2: Shell pipes and features
setTimeout(() => {
  console.log('\n2. Shell pipe command:');
  exec('echo "test" | tr a-z A-Z', (error, stdout, stderr) => {
    if (error) {
      console.error(`   Error: ${error.message}`);
      return;
    }
    console.log(`   Output: ${stdout.trim()}`);
  });
}, 500);

// Example 3: Command with environment variable
setTimeout(() => {
  console.log('\n3. Environment variable expansion:');
  exec('echo "User: $USER"', (error, stdout, stderr) => {
    if (error) {
      console.error(`   Error: ${error.message}`);
      return;
    }
    console.log(`   Output: ${stdout.trim()}`);
  });
}, 1000);

// Example 4: Complex shell command
setTimeout(() => {
  console.log('\n4. Complex shell command:');
  exec(
    'for i in 1 2 3; do echo "Number: $i"; done',
    (error, stdout, stderr) => {
      if (error) {
        console.error(`   Error: ${error.message}`);
        return;
      }
      console.log(`   Output:\n${stdout.trim()}`);
    }
  );
}, 1500);

// Example 5: Error handling
setTimeout(() => {
  console.log('\n5. Error handling (command not found):');
  exec('nonexistent_command', (error, stdout, stderr) => {
    if (error) {
      console.error(`   ✓ Caught error: ${error.message}`);
      console.error(`   Exit code: ${error.code}`);
    }
  });
}, 2000);
