#!/usr/bin/env jsrt
// Basic spawn example - running a simple command

const { spawn } = require('node:child_process');

console.log('=== Basic spawn() Example ===\n');

// Spawn a simple echo command
const child = spawn('echo', ['Hello from child process!']);

console.log(`Spawned child process with PID: ${child.pid}`);

// Capture stdout
child.stdout.on('data', (data) => {
  console.log(`Child stdout: ${data.toString().trim()}`);
});

// Capture stderr
child.stderr.on('data', (data) => {
  console.error(`Child stderr: ${data.toString().trim()}`);
});

// Handle process exit
child.on('exit', (code, signal) => {
  console.log(`\nChild process exited with code ${code} and signal ${signal}`);
});

// Handle errors
child.on('error', (err) => {
  console.error(`Failed to start child process: ${err.message}`);
});
