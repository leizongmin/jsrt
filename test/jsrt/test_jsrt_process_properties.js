// Test new process properties added in Phase 1

console.log('Testing process.execPath...');
if (typeof process.execPath !== 'string') {
  throw new Error('process.execPath should be a string');
}
if (process.execPath.length === 0) {
  throw new Error('process.execPath should not be empty');
}
console.log(`✓ process.execPath = ${process.execPath}`);

console.log('\nTesting process.execArgv...');
if (!Array.isArray(process.execArgv)) {
  throw new Error('process.execArgv should be an array');
}
console.log(`✓ process.execArgv = [${process.execArgv.join(', ')}]`);

console.log('\nTesting process.exitCode...');
// Initially undefined
if (process.exitCode !== undefined) {
  throw new Error('process.exitCode should initially be undefined');
}
console.log('✓ process.exitCode is initially undefined');

// Test setter
process.exitCode = 42;
if (process.exitCode !== 42) {
  throw new Error('process.exitCode should be 42 after setting');
}
console.log('✓ process.exitCode can be set to 42');

// Test reset to undefined
process.exitCode = undefined;
if (process.exitCode !== undefined) {
  throw new Error('process.exitCode should be undefined after reset');
}
console.log('✓ process.exitCode can be reset to undefined');

console.log('\nTesting process.title...');
if (typeof process.title !== 'string') {
  throw new Error('process.title should be a string');
}
console.log(`✓ process.title = ${process.title}`);

// Test setter
const originalTitle = process.title;
process.title = 'test-process';
if (process.title !== 'test-process') {
  throw new Error('process.title should be "test-process" after setting');
}
console.log('✓ process.title can be set to "test-process"');

// Restore original
process.title = originalTitle;
console.log(`✓ process.title restored to ${process.title}`);

console.log('\nTesting process.config...');
if (typeof process.config !== 'object' || process.config === null) {
  throw new Error('process.config should be an object');
}
console.log('✓ process.config is an object');

console.log('\nTesting process.release...');
if (typeof process.release !== 'object' || process.release === null) {
  throw new Error('process.release should be an object');
}
if (process.release.name !== 'jsrt') {
  throw new Error('process.release.name should be "jsrt"');
}
if (typeof process.release.version !== 'string') {
  throw new Error('process.release.version should be a string');
}
console.log(`✓ process.release.name = ${process.release.name}`);
console.log(`✓ process.release.version = ${process.release.version}`);

console.log('\nTesting process.features...');
if (typeof process.features !== 'object' || process.features === null) {
  throw new Error('process.features should be an object');
}
if (typeof process.features.debug !== 'boolean') {
  throw new Error('process.features.debug should be a boolean');
}
if (typeof process.features.uv !== 'boolean') {
  throw new Error('process.features.uv should be a boolean');
}
if (typeof process.features.ipv6 !== 'boolean') {
  throw new Error('process.features.ipv6 should be a boolean');
}
if (typeof process.features.tls !== 'boolean') {
  throw new Error('process.features.tls should be a boolean');
}
console.log(`✓ process.features.debug = ${process.features.debug}`);
console.log(`✓ process.features.uv = ${process.features.uv}`);
console.log(`✓ process.features.ipv6 = ${process.features.ipv6}`);
console.log(`✓ process.features.tls = ${process.features.tls}`);

console.log('\n✅ All process properties tests passed!');
