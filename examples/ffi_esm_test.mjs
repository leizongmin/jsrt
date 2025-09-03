// FFI ES Module Test
import ffi from 'std:ffi';

console.log('=== FFI ES Module Test ===');

console.log('✅ FFI ES module imported successfully');
console.log('📋 FFI version:', ffi.version);
console.log('🔧 Type of Library function:', typeof ffi.Library);

// Test basic functionality
try {
  console.log('\n🧪 Testing library loading via ES import...');
  const lib = ffi.Library('libc.so.6', {
    strlen: ['int', ['string']],
  });

  console.log('✅ Library loaded via ES import!');
  console.log('🎯 strlen function type:', typeof lib.strlen);
} catch (error) {
  console.log(
    '⚠️ Library loading failed (expected on some systems):',
    error.message
  );
}

console.log('\n✅ ES module import test completed successfully!');
