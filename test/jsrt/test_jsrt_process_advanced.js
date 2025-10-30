// Test process advanced features (Phase 6)

const fs = require('node:fs');

console.log('Testing process advanced features...\n');

// Test 1: process.loadEnvFile() - basic functionality
console.log('Test 1: process.loadEnvFile() - basic functionality');
if (typeof process.loadEnvFile === 'function') {
  console.log('✓ process.loadEnvFile() exists');

  // Create a test .env file
  const envPath = 'target/tmp/test.env';
  const envContent = `# Test environment file
TEST_VAR1=value1
TEST_VAR2=value2
TEST_QUOTED="quoted value"
TEST_SINGLE='single quoted'
  TEST_SPACES  =  spaces

# Another comment
TEST_NUMBER=42
`;

  // Ensure target/tmp exists
  try {
    fs.mkdirSync('target/tmp', { recursive: true });
  } catch (e) {
    // Directory might already exist
  }

  // Write test .env file
  fs.writeFileSync(envPath, envContent);

  // Test loading the file
  try {
    process.loadEnvFile(envPath);
    console.log('✓ loadEnvFile() succeeded');

    // Verify variables were loaded
    if (process.env.TEST_VAR1 === 'value1') {
      console.log('✓ TEST_VAR1 loaded correctly');
    } else {
      throw new Error('TEST_VAR1 not loaded correctly');
    }

    if (process.env.TEST_VAR2 === 'value2') {
      console.log('✓ TEST_VAR2 loaded correctly');
    } else {
      throw new Error('TEST_VAR2 not loaded correctly');
    }

    if (process.env.TEST_QUOTED === 'quoted value') {
      console.log('✓ TEST_QUOTED loaded correctly (quotes removed)');
    } else {
      console.log('  Note: TEST_QUOTED =', process.env.TEST_QUOTED);
    }

    if (process.env.TEST_NUMBER === '42') {
      console.log('✓ TEST_NUMBER loaded correctly');
    } else {
      throw new Error('TEST_NUMBER not loaded correctly');
    }

    // Clean up
    fs.unlinkSync(envPath);
  } catch (e) {
    console.error('✗ loadEnvFile() failed:', e.message);
    throw e;
  }
} else {
  throw new Error('process.loadEnvFile not found');
}

// Test 2: process.loadEnvFile() - error handling
console.log('\nTest 2: process.loadEnvFile() - error handling');
try {
  process.loadEnvFile('nonexistent.env');
  throw new Error('Should have thrown for nonexistent file');
} catch (e) {
  if (e.message.includes('Failed to open')) {
    console.log('✓ Throws error for nonexistent file');
  } else {
    throw e;
  }
}

// Test 3: process.getActiveResourcesInfo()
console.log('\nTest 3: process.getActiveResourcesInfo()');
if (typeof process.getActiveResourcesInfo === 'function') {
  console.log('✓ process.getActiveResourcesInfo() exists');

  const resources = process.getActiveResourcesInfo();
  if (Array.isArray(resources)) {
    console.log('✓ getActiveResourcesInfo() returns an array');
    console.log('  Active resources:', resources.length);
    if (resources.length > 0) {
      console.log('  Resource types:', resources.slice(0, 5).join(', '));
    }
  } else {
    throw new Error('getActiveResourcesInfo() should return an array');
  }

  // Create a timer and check if it shows up
  const timer = setTimeout(() => {}, 10000);
  const resourcesWithTimer = process.getActiveResourcesInfo();
  if (resourcesWithTimer.length > resources.length) {
    console.log('✓ Timer resource detected');
  } else {
    console.log('  Note: Timer may not be immediately visible');
  }
  clearTimeout(timer);
} else {
  throw new Error('process.getActiveResourcesInfo not found');
}

// Test 4: process.setSourceMapsEnabled()
console.log('\nTest 4: process.setSourceMapsEnabled()');
if (typeof process.setSourceMapsEnabled === 'function') {
  console.log('✓ process.setSourceMapsEnabled() exists');

  // Test setting to true
  process.setSourceMapsEnabled(true);
  console.log('✓ setSourceMapsEnabled(true) succeeded');

  // Test setting to false
  process.setSourceMapsEnabled(false);
  console.log('✓ setSourceMapsEnabled(false) succeeded');
} else {
  throw new Error('process.setSourceMapsEnabled not found');
}

// Test 5: process.sourceMapsEnabled getter
console.log('\nTest 5: process.sourceMapsEnabled getter');
if (typeof process.sourceMapsEnabled !== 'undefined') {
  console.log('✓ process.sourceMapsEnabled exists');

  // Set and verify
  process.setSourceMapsEnabled(true);
  if (process.sourceMapsEnabled === true) {
    console.log('✓ sourceMapsEnabled returns true after setting');
  } else {
    throw new Error('sourceMapsEnabled should be true');
  }

  process.setSourceMapsEnabled(false);
  if (process.sourceMapsEnabled === false) {
    console.log('✓ sourceMapsEnabled returns false after setting');
  } else {
    throw new Error('sourceMapsEnabled should be false');
  }
} else {
  throw new Error('process.sourceMapsEnabled not found');
}

// Test 6: process.ref()
console.log('\nTest 6: process.ref()');
if (typeof process.ref === 'function') {
  console.log('✓ process.ref() exists');

  // Just test that it doesn't throw
  process.ref();
  console.log('✓ process.ref() succeeded');
} else {
  throw new Error('process.ref not found');
}

// Test 7: process.unref()
console.log('\nTest 7: process.unref()');
if (typeof process.unref === 'function') {
  console.log('✓ process.unref() exists');

  // Just test that it doesn't throw
  process.unref();
  console.log('✓ process.unref() succeeded');
} else {
  throw new Error('process.unref not found');
}

// Test 8: ref/unref interaction with event loop
console.log('\nTest 8: ref/unref interaction');
// This is a basic test - full event loop control testing would require
// separate test files that exit based on ref/unref behavior
process.ref();
process.unref();
console.log('✓ ref/unref calls work together');

console.log('\n✅ All process advanced feature tests passed!');
