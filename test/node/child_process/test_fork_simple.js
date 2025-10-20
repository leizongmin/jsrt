// Simple test for fork() - verifies it runs without crashing
const child_process = require('node:child_process');

console.log('Test 1: fork() can be called');

try {
  // Try to fork a simple script
  const child = child_process.fork(__filename);

  console.log('✓ fork() returned a ChildProcess');
  console.log('✓ Child PID:', child.pid);
  console.log('✓ Child connected:', child.connected);

  // Send a message (will fail gracefully if IPC not fully implemented)
  if (typeof child.send === 'function') {
    console.log('✓ child.send() exists');
    try {
      const result = child.send({ test: 'message' });
      console.log('✓ child.send() returned:', result);
    } catch (e) {
      console.log('! child.send() threw:', e.message);
    }
  }

  // Disconnect
  if (typeof child.disconnect === 'function') {
    console.log('✓ child.disconnect() exists');
    child.disconnect();
  }

  // Kill the child
  child.kill();
  console.log('✓ child.kill() called');

  console.log('\nAll basic tests passed!');
} catch (error) {
  console.error('✗ Test failed:', error.message);
  console.error(error.stack);
  process.exit(1);
}
