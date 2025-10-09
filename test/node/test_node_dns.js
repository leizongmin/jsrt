/**
 * Test: node:dns module
 * Tests DNS lookup functionality using callback-based API and promises API
 */

const dns = require('node:dns');

console.log('Testing node:dns module...\n');

let passedTests = 0;
let failedTests = 0;
let pendingTests = 5; // Number of async tests

function reportTest(name, passed, error) {
  if (passed) {
    console.log(`✓ ${name}`);
    passedTests++;
  } else {
    console.log(`✗ ${name}: ${error}`);
    failedTests++;
  }

  pendingTests--;
  if (pendingTests === 0) {
    console.log(`\n${passedTests} passed, ${failedTests} failed`);
    if (failedTests > 0) {
      process.exit(1);
    }
  }
}

// Test 1: dns.lookup() with localhost (callback version)
dns.lookup('localhost', (err, address, family) => {
  if (err) {
    reportTest('dns.lookup("localhost") callback', false, err.message);
    return;
  }

  const validAddresses = ['127.0.0.1', '::1'];
  const addressValid = validAddresses.includes(address);
  const familyValid = family === 4 || family === 6;

  if (addressValid && familyValid) {
    console.log(`  Resolved: ${address} (family: ${family})`);
    reportTest('dns.lookup("localhost") callback', true);
  } else {
    reportTest(
      'dns.lookup("localhost") callback',
      false,
      `Invalid result: ${address}, family: ${family}`
    );
  }
});

// Test 2: dns.lookup() with family option
dns.lookup('localhost', { family: 4 }, (err, address, family) => {
  if (err) {
    reportTest('dns.lookup("localhost", {family: 4})', false, err.message);
    return;
  }

  if (address === '127.0.0.1' && family === 4) {
    console.log(`  Resolved: ${address} (family: ${family})`);
    reportTest('dns.lookup("localhost", {family: 4})', true);
  } else {
    reportTest(
      'dns.lookup("localhost", {family: 4})',
      false,
      `Expected 127.0.0.1/4, got ${address}/${family}`
    );
  }
});

// Test 3: dns.lookup() with all option
dns.lookup('localhost', { all: true }, (err, addresses) => {
  if (err) {
    reportTest('dns.lookup("localhost", {all: true})', false, err.message);
    return;
  }

  if (Array.isArray(addresses) && addresses.length > 0) {
    const hasValidEntry = addresses.some(
      (addr) =>
        (addr.address === '127.0.0.1' || addr.address === '::1') &&
        (addr.family === 4 || addr.family === 6)
    );
    if (hasValidEntry) {
      console.log(`  Resolved: ${addresses.length} address(es)`);
      reportTest('dns.lookup("localhost", {all: true})', true);
    } else {
      reportTest(
        'dns.lookup("localhost", {all: true})',
        false,
        'No valid addresses found'
      );
    }
  } else {
    reportTest(
      'dns.lookup("localhost", {all: true})',
      false,
      'Expected array of addresses'
    );
  }
});

// Test 4: dns.lookup() error handling
dns.lookup('invalid.hostname.that.does.not.exist.xyz.test', (err, address) => {
  if (err) {
    if (err.code === 'ENOTFOUND' && err.syscall === 'getaddrinfo') {
      console.log(`  Error code: ${err.code}, syscall: ${err.syscall}`);
      reportTest('dns.lookup() error handling', true);
    } else {
      reportTest(
        'dns.lookup() error handling',
        false,
        `Wrong error: ${err.code}`
      );
    }
  } else {
    reportTest(
      'dns.lookup() error handling',
      false,
      'Expected error but got success'
    );
  }
});

// Test 5: dns.promises.lookup() - promise API
const dnsPromises = dns.promises;
if (dnsPromises && typeof dnsPromises.lookup === 'function') {
  dnsPromises
    .lookup('localhost')
    .then((result) => {
      if (result.address === '127.0.0.1' || result.address === '::1') {
        console.log(`  Promise resolved: ${result.address}`);
        reportTest('dns.promises.lookup("localhost")', true);
      } else {
        reportTest(
          'dns.promises.lookup("localhost")',
          false,
          `Invalid address: ${result.address}`
        );
      }
    })
    .catch((err) => {
      reportTest('dns.promises.lookup("localhost")', false, err.message);
    });
} else {
  reportTest(
    'dns.promises.lookup("localhost")',
    false,
    'dns.promises API not available'
  );
}
