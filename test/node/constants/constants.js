#!/usr/bin/env node

/**
 * Basic Constants Module Tests
 * Tests fundamental functionality of the node:constants module
 */

console.log('🧪 Basic Constants Module Tests\n');

try {
  // Test basic module loading with node:constants
  console.log('Testing module loading...');
  const constants = require('node:constants');
  console.log('✅ node:constants module loaded successfully');

  // Test ESM imports (if supported)
  try {
    // This would be tested with a separate ESM test file
    console.log('ℹ️  ESM imports will be tested separately');
  } catch (error) {
    console.log('ℹ️  ESM import test skipped');
  }

  // Test that essential properties exist
  console.log('\nTesting essential properties...');

  const requiredProps = [
    'errno',
    'signals',
    'priority',
    'F_OK',
    'R_OK',
    'W_OK',
    'X_OK',
  ];
  let missingProps = [];

  for (const prop of requiredProps) {
    if (!(prop in constants)) {
      missingProps.push(prop);
    }
  }

  if (missingProps.length === 0) {
    console.log('✅ All essential properties present');
  } else {
    console.log(`❌ Missing properties: ${missingProps.join(', ')}`);
    process.exit(1);
  }

  // Test errno constants
  console.log('\nTesting errno constants...');
  const essentialErrno = ['ENOENT', 'EACCES', 'EEXIST', 'EINVAL'];
  let errnoFailures = [];

  for (const err of essentialErrno) {
    if (!(err in constants.errno)) {
      errnoFailures.push(err);
    } else if (typeof constants.errno[err] !== 'number') {
      errnoFailures.push(`${err} (not a number)`);
    }
  }

  if (errnoFailures.length === 0) {
    console.log('✅ Essential errno constants present and numeric');
    console.log(`   ENOENT: ${constants.errno.ENOENT}`);
    console.log(`   EACCES: ${constants.errno.EACCES}`);
  } else {
    console.log(`❌ Errno failures: ${errnoFailures.join(', ')}`);
    process.exit(1);
  }

  // Test signals constants
  console.log('\nTesting signal constants...');
  const essentialSignals = ['SIGTERM', 'SIGINT', 'SIGKILL', 'SIGUSR1'];
  let signalFailures = [];

  for (const sig of essentialSignals) {
    if (!(sig in constants.signals)) {
      signalFailures.push(sig);
    } else if (typeof constants.signals[sig] !== 'number') {
      signalFailures.push(`${sig} (not a number)`);
    }
  }

  if (signalFailures.length === 0) {
    console.log('✅ Essential signal constants present and numeric');
    console.log(`   SIGTERM: ${constants.signals.SIGTERM}`);
    console.log(`   SIGINT: ${constants.signals.SIGINT}`);
  } else {
    console.log(`❌ Signal failures: ${signalFailures.join(', ')}`);
    process.exit(1);
  }

  // Test file access constants
  console.log('\nTesting file access constants...');
  const accessConstants = ['F_OK', 'R_OK', 'W_OK', 'X_OK'];
  let accessFailures = [];

  for (const access of accessConstants) {
    if (!(access in constants) || typeof constants[access] !== 'number') {
      accessFailures.push(access);
    }
  }

  if (accessFailures.length === 0) {
    console.log('✅ File access constants present and numeric');
    console.log(`   F_OK: ${constants.F_OK}, R_OK: ${constants.R_OK}`);
    console.log(`   W_OK: ${constants.W_OK}, X_OK: ${constants.X_OK}`);
  } else {
    console.log(`❌ Access failures: ${accessFailures.join(', ')}`);
    process.exit(1);
  }

  // Test priority constants
  console.log('\nTesting priority constants...');
  if (constants.priority && typeof constants.priority === 'object') {
    const priorityProps = Object.keys(constants.priority);
    console.log(
      `✅ Priority object found with ${priorityProps.length} properties`
    );
    if (priorityProps.length > 0) {
      console.log(`   Properties: ${priorityProps.join(', ')}`);
    }
  } else {
    console.log('❌ Priority constants not found or not an object');
    process.exit(1);
  }

  // Test extended file system categories
  console.log('\nTesting extended file system categories...');
  const fsCategories = ['faccess', 'fopen', 'filetype', 'permissions'];
  const foundCategories = [];

  for (const category of fsCategories) {
    if (constants[category] && typeof constants[category] === 'object') {
      foundCategories.push(category);
      const count = Object.keys(constants[category]).length;
      console.log(`✅ ${category}: ${count} constants`);
    } else {
      console.log(`⚠️  ${category}: not found or not an object`);
    }
  }

  // Test crypto constants
  console.log('\nTesting crypto constants...');
  if (constants.crypto && typeof constants.crypto === 'object') {
    const cryptoProps = Object.keys(constants.crypto);
    console.log(`✅ Crypto object found with ${cryptoProps.length} properties`);
    if (cryptoProps.length > 0) {
      console.log(
        `   Properties: ${cryptoProps.slice(0, 5).join(', ')}${cryptoProps.length > 5 ? '...' : ''}`
      );
    }
  } else {
    console.log('⚠️  Crypto constants not found or not an object');
  }

  // Test module identity (should be the same object)
  console.log('\nTesting module identity...');
  const constants1 = require('node:constants');
  const constants2 = require('constants'); // This should also work if implemented

  if (constants1 === constants2) {
    console.log('✅ require("node:constants") === require("constants")');
  } else {
    console.log(
      'ℹ️  require("node:constants") !== require("constants") (may be expected)'
    );
  }

  // Summary
  console.log('\n📊 Basic Test Summary:');
  console.log(`✅ Module Loading: node:constants`);
  console.log(`✅ Essential Properties: ${requiredProps.length}`);
  console.log(`✅ Errno Constants: ${essentialErrno.length}`);
  console.log(`✅ Signal Constants: ${essentialSignals.length}`);
  console.log(`✅ File Access Constants: ${accessConstants.length}`);
  console.log(`✅ Priority Object: present`);
  console.log(`✅ Extended Categories: ${foundCategories.length}`);
  console.log(`✅ Overall: SUCCESS`);
} catch (error) {
  console.error('❌ Test failed:', error.message);
  console.error('Stack:', error.stack);
  process.exit(1);
}
