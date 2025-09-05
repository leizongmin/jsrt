// Example of basic WebAssembly usage in jsrt
console.log("=== WebAssembly Example ===");

// Check if WebAssembly is available
if (typeof WebAssembly === 'undefined') {
    console.log("❌ WebAssembly is not available");
    process.exit(1);
}

console.log("✅ WebAssembly global object is available");

// Create a minimal valid WebAssembly module
// This is the minimal WASM module that just has magic number and version
const wasmBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, // WASM magic number
    0x01, 0x00, 0x00, 0x00  // WASM version
]);

// Test validation
console.log("\n--- WebAssembly.validate() ---");
const isValid = WebAssembly.validate(wasmBytes);
console.log("Is valid WASM:", isValid);

// Test module creation
console.log("\n--- WebAssembly.Module() ---");
try {
    const module = new WebAssembly.Module(wasmBytes);
    console.log("✅ Module created successfully");
    
    // Test instance creation
    console.log("\n--- WebAssembly.Instance() ---");
    try {
        const instance = new WebAssembly.Instance(module);
        console.log("✅ Instance created successfully");
        console.log("Instance exports:", Object.keys(instance.exports || {}));
    } catch (error) {
        console.log("⚠️ Instance creation failed:", error.message);
        console.log("Note: Minimal WASM module might not be instantiable");
    }
} catch (error) {
    console.log("❌ Module creation failed:", error.message);
}

// Test invalid data
console.log("\n--- Testing error handling ---");
try {
    const invalidModule = new WebAssembly.Module(new Uint8Array([1, 2, 3, 4]));
    console.log("❌ Should have failed");
} catch (error) {
    console.log("✅ Correctly rejected invalid WASM:", error.message);
}

console.log("\n=== WebAssembly Example Complete ===");