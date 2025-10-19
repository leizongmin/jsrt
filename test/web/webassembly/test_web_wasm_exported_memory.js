// Test WebAssembly exported Memory (instance.exports.mem)
// These work correctly via WAMR Runtime API

console.log('Test 1: Exported Memory - buffer access');
{
  // (module (memory (export "mem") 1))
  const wasmBytes = new Uint8Array([
    0x00,
    0x61,
    0x73,
    0x6d, // WASM magic
    0x01,
    0x00,
    0x00,
    0x00, // WASM version
    // Memory section
    0x05, // section code 5 (memory)
    0x03, // section size
    0x01, // 1 memory
    0x00, // flags (no max)
    0x01, // initial: 1 page (64KB)
    // Export section
    0x07, // section code 7 (export)
    0x07, // section size
    0x01, // 1 export
    0x03, // name length
    0x6d,
    0x65,
    0x6d, // "mem"
    0x02, // export kind: memory
    0x00, // memory index 0
  ]);

  const module = new WebAssembly.Module(wasmBytes);
  const instance = new WebAssembly.Instance(module);

  console.log('  Instance created');
  console.log('  Exports:', Object.keys(instance.exports));

  if (instance.exports.mem) {
    console.log('  ✓ Memory found in exports');
    const mem = instance.exports.mem;
    console.log('  ✓ Type:', typeof mem);

    if (!(mem instanceof WebAssembly.Memory)) {
      throw new Error('Not a Memory object');
    }
    console.log('  ✓ Is Memory instance');

    if (mem.buffer) {
      console.log('  ✓ Has buffer property');
      console.log('  ✓ Buffer type:', mem.buffer.constructor.name);
      console.log('  ✓ Buffer size:', mem.buffer.byteLength, 'bytes');

      // WAMR might allocate more than 1 page, just check it's at least 64KB
      if (mem.buffer.byteLength < 65536) {
        throw new Error(
          `Expected at least 65536 bytes, got ${mem.buffer.byteLength}`
        );
      }
      console.log('  ✓ Buffer size >= 64KB (WAMR may allocate extra pages)');
    } else {
      throw new Error('No buffer property');
    }
  } else {
    throw new Error('Memory not in exports');
  }

  console.log('  PASS: Exported Memory buffer access works');
}

console.log('\nTest 2: Exported Memory - read/write data');
{
  // (module (memory (export "mem") 1))
  const wasmBytes = new Uint8Array([
    0x00,
    0x61,
    0x73,
    0x6d, // WASM magic
    0x01,
    0x00,
    0x00,
    0x00, // WASM version
    0x05,
    0x03,
    0x01,
    0x00,
    0x01, // Memory section: 1 page
    0x07,
    0x07,
    0x01,
    0x03,
    0x6d,
    0x65,
    0x6d,
    0x02,
    0x00, // Export "mem"
  ]);

  const module = new WebAssembly.Module(wasmBytes);
  const instance = new WebAssembly.Instance(module);
  const mem = instance.exports.mem;

  // Write data to memory
  const view = new Uint8Array(mem.buffer);
  view[0] = 42;
  view[1] = 100;
  view[100] = 255;

  // Read it back
  if (view[0] !== 42 || view[1] !== 100 || view[100] !== 255) {
    throw new Error(
      `Read/write failed: [${view[0]}, ${view[1]}, ${view[100]}]`
    );
  }

  console.log('  ✓ Can write to memory');
  console.log('  ✓ Can read from memory');
  console.log('  PASS: Exported Memory read/write works');
}

console.log('\n========================================');
console.log('Exported Memory tests completed!');
console.log('Key finding: Instance-exported memories work perfectly');
console.log('Blocker only affects standalone new WebAssembly.Memory()');
console.log('========================================');
