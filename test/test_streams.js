// Test Streams API
console.log("Testing Streams API...");

// Test ReadableStream
try {
  const readable = new ReadableStream();
  console.log("✓ ReadableStream constructor works");
  
  console.log("ReadableStream locked:", readable.locked);
  
  const reader = readable.getReader();
  console.log("✓ getReader() works");
  console.log("ReadableStream locked after getReader():", readable.locked);
} catch (error) {
  console.error("✗ ReadableStream error:", error.message);
}

// Test WritableStream  
try {
  const writable = new WritableStream();
  console.log("✓ WritableStream constructor works");
  
  console.log("WritableStream locked:", writable.locked);
} catch (error) {
  console.error("✗ WritableStream error:", error.message);
}

// Test TransformStream
try {
  const transform = new TransformStream();
  console.log("✓ TransformStream constructor works");
  
  console.log("TransformStream has readable:", !!transform.readable);
  console.log("TransformStream has writable:", !!transform.writable);
} catch (error) {
  console.error("✗ TransformStream error:", error.message);
}