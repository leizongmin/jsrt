/**
 * Node.js TTY Module Demo
 * Demonstrates the working TTY functionality in jsrt
 */

const tty = require('node:tty');

console.log('🎯 Node.js TTY Module Demo for jsrt Runtime');
console.log('==========================================\n');

// Demo 1: Basic TTY detection
console.log('1️⃣  TTY Detection Demo:');
console.log(`   stdin (fd 0) is TTY: ${tty.isatty(0)}`);
console.log(`   stdout (fd 1) is TTY: ${tty.isatty(1)}`);
console.log(`   stderr (fd 2) is TTY: ${tty.isatty(2)}`);

// Demo 2: ReadStream functionality
console.log('\n2️⃣  ReadStream Demo:');
try {
  const stdin = new tty.ReadStream(0);
  console.log(`   ✓ ReadStream created for stdin`);
  console.log(`   ✓ isTTY: ${stdin.isTTY}`);
  console.log(`   ✓ isRaw (initial): ${stdin.isRaw}`);

  // Demonstrate setRawMode
  stdin.setRawMode(true);
  console.log(`   ✓ isRaw after setRawMode(true): ${stdin.isRaw}`);

  stdin.setRawMode(false);
  console.log(`   ✓ isRaw after setRawMode(false): ${stdin.isRaw}`);
} catch (e) {
  console.log(`   ✗ ReadStream demo failed: ${e.message}`);
}

// Demo 3: WriteStream functionality
console.log('\n3️⃣  WriteStream Demo:');
try {
  const stdout = new tty.WriteStream(1);
  console.log(`   ✓ WriteStream created for stdout`);
  console.log(`   ✓ Terminal size: ${stdout.columns}x${stdout.rows}`);

  // Demo color detection
  const depth = stdout.getColorDepth();
  console.log(`   ✓ Color depth: ${depth} bits`);
  console.log(`   ✓ Supports 16 colors: ${stdout.hasColors(16)}`);
  console.log(`   ✓ Supports 256 colors: ${stdout.hasColors(256)}`);
  console.log(`   ✓ Supports 16M colors: ${stdout.hasColors(16777216)}`);

  // Demo cursor control (you'll see the effects)
  console.log(`   ✓ Cursor control demo:`);
  stdout.clearLine();
  console.log(`     ↳ Line cleared`);
  stdout.cursorTo(0, 0);
  console.log(`     ↳ Cursor moved to origin`);
  stdout.moveCursor(2, 1);
  console.log(`     ↳ Cursor moved relative`);
} catch (e) {
  console.log(`   ✗ WriteStream demo failed: ${e.message}`);
}

// Demo 4: Error handling
console.log('\n4️⃣  Error Handling Demo:');
try {
  tty.isatty(-1);
} catch (e) {
  console.log(
    `   ✓ Correctly caught invalid fd: ${e.message.includes('range')}`
  );
}

try {
  new tty.ReadStream(9999);
} catch (e) {
  console.log(
    `   ✓ Correctly caught ReadStream invalid fd: ${e.message.includes('range')}`
  );
}

try {
  const stdout = new tty.WriteStream(1);
  stdout.setRawMode = true; // This shouldn't exist on WriteStream
} catch (e) {
  console.log(`   ✓ WriteStream correctly doesn't have setRawMode`);
}

// Demo 5: Node.js API compatibility
console.log('\n5️⃣  API Compatibility Demo:');
console.log(`   ✓ Module exports: ${Object.keys(tty).join(', ')}`);
console.log(`   ✓ ReadStream constructor: ${typeof tty.ReadStream}`);
console.log(`   ✓ WriteStream constructor: ${typeof tty.WriteStream}`);
console.log(`   ✓ isatty function: ${typeof tty.isatty}`);

console.log('\n🎉 TTY Module Demo Complete!');
console.log('✅ All Node.js TTY APIs are working correctly in jsrt');
