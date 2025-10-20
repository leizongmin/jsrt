// Child process for IPC testing

// Check if we're running in a forked context
if (typeof process !== 'undefined' && process.send) {
  console.log('Child: IPC channel detected');

  // Send initial message
  process.send({ type: 'ready', pid: process.pid });

  // Listen for messages from parent
  process.on('message', (msg) => {
    console.log('Child received:', JSON.stringify(msg));

    if (msg.type === 'echo') {
      // Echo back
      process.send({ type: 'echo-response', data: msg.data });
    } else if (msg.type === 'ping') {
      // Respond with pong
      process.send({ type: 'pong' });
    } else if (msg.type === 'exit') {
      // Exit cleanly
      process.send({ type: 'goodbye' });
      process.exit(0);
    }
  });

  // Handle disconnect
  process.on('disconnect', () => {
    console.log('Child: disconnected from parent');
    process.exit(0);
  });
} else {
  console.error('No IPC channel - not running as forked child');
  process.exit(1);
}
