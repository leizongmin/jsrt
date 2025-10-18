// Tests for WebAssembly.instantiate promise API

const wasmBytes = new Uint8Array([
  0x00,
  0x61,
  0x73,
  0x6d, // magic
  0x01,
  0x00,
  0x00,
  0x00, // version
  // Type section: func returning i32
  0x01,
  0x05,
  0x01,
  0x60,
  0x00,
  0x01,
  0x7f,
  // Function section
  0x03,
  0x02,
  0x01,
  0x00,
  // Export section: "answer"
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
  // Code section: returns 42
  0x0a,
  0x06,
  0x01,
  0x04,
  0x00,
  0x41,
  0x2a,
  0x0b,
]);

const invalidWasm = new Uint8Array([0x00, 0x61, 0x73]);

async function testInstantiateBytes() {
  console.log(
    'Test 1: instantiate(BufferSource) resolves with {module, instance}'
  );
  const result = await WebAssembly.instantiate(wasmBytes);
  if (typeof result !== 'object' || result === null) {
    throw new Error('instantiate should resolve to an object');
  }
  if (!(result.module instanceof WebAssembly.Module)) {
    throw new Error('result.module should be a WebAssembly.Module');
  }
  if (!(result.instance instanceof WebAssembly.Instance)) {
    throw new Error('result.instance should be a WebAssembly.Instance');
  }
  const value = result.instance.exports.answer();
  if (value !== 42) {
    throw new Error(`Expected export to return 42, got ${value}`);
  }
  console.log('  PASS: instantiate(bytes) returns module and working instance');
}

async function testInstantiateInvalid() {
  console.log('\nTest 2: instantiate rejects invalid bytes with CompileError');
  let rejected = false;
  try {
    await WebAssembly.instantiate(invalidWasm);
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
    throw new Error('instantiate should reject invalid bytes');
  }
}

async function testInstantiateModuleOverload() {
  console.log('\nTest 3: instantiate(Module) resolves with Instance');
  const module = new WebAssembly.Module(wasmBytes);
  const instance = await WebAssembly.instantiate(module);
  if (!(instance instanceof WebAssembly.Instance)) {
    throw new Error('instantiate(module) should resolve to an Instance');
  }
  const value = instance.exports.answer();
  if (value !== 42) {
    throw new Error(`Expected export to return 42, got ${value}`);
  }
  console.log('  PASS: instantiate(module) returns working instance');
}

async function run() {
  await testInstantiateBytes();
  await testInstantiateInvalid();
  await testInstantiateModuleOverload();
  console.log('\nAll WebAssembly.instantiate async tests passed!');
}

run()
  .then(() => {
    console.log('Done.');
  })
  .catch((err) => {
    console.error('FAIL:', err && err.stack ? err.stack : err);
    throw err;
  });
