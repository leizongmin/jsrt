#!/usr/bin/env jsrt

import { createServer } from 'node:http';

console.log('Starting HTTP server...');

// Create HTTP server
const server = createServer((req, res) => {
  console.log(`📨 ${req.method} ${req.url}`);

  res.writeHead(200, {
    'Content-Type': 'text/plain',
    Server: 'jsrt/1.0',
  });

  res.end(
    `Hello from jsrt!\nYou requested: ${req.url}\nMethod: ${req.method}\n`
  );
});

// Start server - this is now non-blocking!
server.listen(3000, '127.0.0.1', () => {
  console.log('✅ HTTP server listening on http://127.0.0.1:3000/');
  console.log('Try: curl http://127.0.0.1:3000/test');
});

console.log('🎯 Server setup initiated - execution continues...');

// Demonstrate non-blocking by showing this executes immediately
setTimeout(() => {
  console.log('⏰ Timer executed - event loop is working!');
}, 100);

// Keep server running
process.on('SIGINT', () => {
  console.log('\n🛑 Shutting down server...');
  server.close(() => {
    console.log('✅ Server closed gracefully');
    process.exit(0);
  });
});
