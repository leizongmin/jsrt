// Demo of the FFI (Foreign Function Interface) module
console.log('🚀 FFI Module Demo');
console.log('==================');

// Load the FFI module
const ffi = require('std:ffi');

console.log('✅ FFI module loaded successfully');
console.log('📋 FFI version:', ffi.version);
console.log('🔧 Available methods:', Object.keys(ffi));

// Demo: Loading a library (without actually calling functions)
try {
  console.log('\n📚 Loading system library...');
  
  // Try to load a common system library
  const lib = ffi.Library('libc.so.6', {
    'strlen': ['int', ['string']],
    'strcmp': ['int', ['string', 'string']],
    'malloc': ['pointer', ['int']],
    'free': ['void', ['pointer']]
  });
  
  console.log('✅ Library loaded successfully!');
  console.log('🎯 Available functions:');
  
  // List the functions that were successfully loaded
  for (const key of Object.keys(lib)) {
    if (typeof lib[key] === 'function') {
      console.log(`   - ${key}(): ${typeof lib[key]}`);
    }
  }
  
} catch (error) {
  console.log('⚠️ Library loading failed:', error.message);
  console.log('   This is normal on some systems without the exact library name');
}

// Demo: Show error handling for non-existent library
console.log('\n🧪 Testing error handling...');
try {
  ffi.Library('nonexistent_library.so', {
    'dummy': ['void', []]
  });
  console.log('❌ This should not succeed');
} catch (error) {
  console.log('✅ Error correctly caught:', error.message);
}

console.log('\n📖 Usage Examples:');
console.log('const ffi = require("std:ffi");');
console.log('const lib = ffi.Library("mylibrary.so", {');
console.log('  "my_function": ["int", ["string", "int"]]');
console.log('});');
console.log('// result = lib.my_function("hello", 42);');

console.log('\n⚠️  Important Notes:');
console.log('   - This is a proof-of-concept implementation');
console.log('   - Library loading and function binding work');
console.log('   - Actual function calls are not fully implemented');
console.log('   - See docs/ffi.md for complete documentation');

console.log('\n🎉 FFI Demo completed successfully!');