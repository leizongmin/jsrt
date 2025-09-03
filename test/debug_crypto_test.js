console.log('=== Crypto Debug Test ===');
console.log('typeof crypto:', typeof crypto);
console.log('typeof process:', typeof process);

// Check global object
console.log('typeof globalThis:', typeof globalThis);
if (typeof globalThis !== 'undefined') {
  console.log('globalThis.process:', typeof globalThis.process);
}

if (typeof process !== 'undefined') {
  console.log('process defined');
  console.log('process.versions:', typeof process.versions);
  if (process.versions) {
    console.log('process.versions content:', process.versions);
  }
} else {
  console.log('process is undefined');
}

if (typeof crypto !== 'undefined') {
  console.log('crypto.subtle:', typeof crypto.subtle);
  if (crypto.subtle) {
    console.log('crypto.subtle.digest:', typeof crypto.subtle.digest);
  }

  // Test getRandomValues to see if crypto works at all
  try {
    const arr = new Uint8Array(4);
    crypto.getRandomValues(arr);
    console.log('crypto.getRandomValues works:', Array.from(arr));
  } catch (e) {
    console.log('crypto.getRandomValues failed:', e.message);
  }
}
