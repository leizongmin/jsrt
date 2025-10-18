// Tests for WebAssembly.compile promise API

const validWasm = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  // Type section: one function returning i32
  0x01,
  0x05,
  0x01,
  0x60,
  0x00,
  0x01,
  0x7f,
  // Function section: single function uses type 0
  0x03,
  0x02,
  0x01,
  0x00,
  // Export section: export "answer"
  0x07,
  0x09,
  0x01,
  0x06,
  0x61,
  0x6e,
  0x73,
  0x77,
  0x65,
  0x72,
  0x00,
  0x00,
  // Code section: i32.const 42
  0x0a,
  0x06,
  0x01,
  0x04,
  0x00,
  0x41,
  0x2a,
  0x0b,
]);

const invalidWasm = new Uint8Array([0x00, 0x61, 0x73]); // Too short

async function run() {
  console.log('Test 1: WebAssembly.compile resolves with Module');
  const module = await WebAssembly.compile(validWasm);
  if (!(module instanceof WebAssembly.Module)) {
    throw new Error('compile should resolve to a WebAssembly.Module');
  }
  console.log('  PASS: Resolved value is WebAssembly.Module');

  console.log('\nTest 2: WebAssembly.compile rejects invalid bytes');
  let rejected = false;
  try {
    await WebAssembly.compile(invalidWasm);
  } catch (err) {
    rejected = true;
    if (!(err instanceof WebAssembly.CompileError)) {
      throw new Error(
        `Expected CompileError, got ${err && err.constructor && err.constructor.name}`
      );
    }
    console.log('  PASS: Rejected with CompileError');
  }
  if (!rejected) {
    throw new Error('compile should reject invalid bytes');
  }

  console.log('\nAll WebAssembly.compile async tests passed!');
}

run()
  .then(() => {
    console.log('Done.');
  })
  .catch((err) => {
    console.error('FAIL:', err && err.stack ? err.stack : err);
    throw err;
  });
