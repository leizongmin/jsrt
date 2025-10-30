// Test multiple listeners precisely

console.log('=== Test: Multiple listeners for same event ===\n');

let count = 0;

console.log('Step 1: Adding first listener');
process.on('multiTest', function () {
  console.log('  First listener executing');
  count++;
  console.log('  Count is now:', count);
});

console.log('Step 2: Adding second listener');
process.on('multiTest', function () {
  console.log('  Second listener executing');
  count++;
  console.log('  Count is now:', count);
});

console.log('Step 3: Emitting multiTest event');
const result = process.emit('multiTest');
console.log('Step 4: Emit returned:', result);
console.log('Step 5: Final count:', count);

if (count === 2) {
  console.log('\n✅ Test passed!');
} else {
  console.log('\n✗ Test failed: expected count 2, got', count);
  throw new Error('Test failed');
}
