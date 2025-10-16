// Test WebAssembly class infrastructure

// Test Module class
const validWasm = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
]);
const module = new WebAssembly.Module(validWasm);

// Test instanceof
if (!(module instanceof WebAssembly.Module)) {
  throw new Error('Module instanceof check failed');
}

// Test toStringTag
const moduleTag = Object.prototype.toString.call(module);
if (moduleTag !== '[object WebAssembly.Module]') {
  throw new Error(`Module toStringTag wrong: ${moduleTag}`);
}

// Test Instance class
const instance = new WebAssembly.Instance(module);

if (!(instance instanceof WebAssembly.Instance)) {
  throw new Error('Instance instanceof check failed');
}

const instanceTag = Object.prototype.toString.call(instance);
if (instanceTag !== '[object WebAssembly.Instance]') {
  throw new Error(`Instance toStringTag wrong: ${instanceTag}`);
}

console.log('All WebAssembly class infrastructure tests passed!');
