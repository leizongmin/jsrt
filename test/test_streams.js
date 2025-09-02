// Test Streams API
const assert = require("std:assert");
console.log("Testing Streams API...");

// Test ReadableStream
const readable = new ReadableStream();
assert.ok(readable, 'ReadableStream constructor should work');
console.log("✓ ReadableStream constructor works");

assert.strictEqual(readable.locked, false, 'ReadableStream should initially be unlocked');
console.log("ReadableStream locked:", readable.locked);

const reader = readable.getReader();
assert.ok(reader, 'getReader() should return a reader');
console.log("✓ getReader() works");

assert.strictEqual(readable.locked, true, 'ReadableStream should be locked after getReader()');
console.log("ReadableStream locked after getReader():", readable.locked);

// Test WritableStream  
const writable = new WritableStream();
assert.ok(writable, 'WritableStream constructor should work');
console.log("✓ WritableStream constructor works");

assert.strictEqual(writable.locked, false, 'WritableStream should initially be unlocked');
console.log("WritableStream locked:", writable.locked);

// Test TransformStream
const transform = new TransformStream();
assert.ok(transform, 'TransformStream constructor should work');
console.log("✓ TransformStream constructor works");

assert.ok(transform.readable, 'TransformStream should have readable property');
console.log("TransformStream has readable:", !!transform.readable);

assert.ok(transform.writable, 'TransformStream should have writable property');
console.log("TransformStream has writable:", !!transform.writable);