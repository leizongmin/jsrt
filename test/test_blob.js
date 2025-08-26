// Test Blob API
console.log("Testing Blob API...");

// Test basic Blob constructor
try {
  const blob1 = new Blob();
  console.log("✓ Empty Blob constructor works");
  console.log("Empty blob size:", blob1.size);
  console.log("Empty blob type:", blob1.type);
} catch (error) {
  console.error("✗ Empty Blob error:", error.message);
}

// Test Blob with string array
try {
  const blob2 = new Blob(["hello", " world"]);
  console.log("✓ Blob with string array works");
  console.log("String blob size:", blob2.size);
  console.log("String blob type:", blob2.type);
} catch (error) {
  console.error("✗ String Blob error:", error.message);
}

// Test Blob with options
try {
  const blob3 = new Blob(["test"], {type: "text/plain"});
  console.log("✓ Blob with options works");
  console.log("Typed blob size:", blob3.size);
  console.log("Typed blob type:", blob3.type);
} catch (error) {
  console.error("✗ Typed Blob error:", error.message);
}

// Test Blob methods
try {
  const blob4 = new Blob(["hello world"]);
  
  // Test slice
  const sliced = blob4.slice(0, 5);
  console.log("✓ Blob.slice() works");
  console.log("Sliced blob size:", sliced.size);
  
  // Test text() - now returns string directly
  const text = blob4.text();
  console.log("✓ Blob.text() works:", text);
  
  // Test arrayBuffer() - now returns ArrayBuffer directly
  const buffer = blob4.arrayBuffer();
  console.log("✓ Blob.arrayBuffer() works, size:", buffer.byteLength);
  
  // Test stream() - should create ReadableStream
  try {
    const stream = blob4.stream();
    console.log("✓ Blob.stream() works");
  } catch (err) {
    console.log("⚠ Blob.stream() error (expected if ReadableStream not available):", err.message);
  }
  
} catch (error) {
  console.error("✗ Blob methods error:", error.message);
}