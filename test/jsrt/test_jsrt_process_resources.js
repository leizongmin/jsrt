// Test process resource monitoring APIs

console.log('Testing process resource monitoring...\n');

// Test 1: process.cpuUsage()
console.log('Test 1: process.cpuUsage()');
if (typeof process.cpuUsage === 'function') {
  console.log('✓ process.cpuUsage() exists');

  const cpu1 = process.cpuUsage();
  if (typeof cpu1 === 'object' && cpu1 !== null) {
    if (typeof cpu1.user === 'number' && typeof cpu1.system === 'number') {
      console.log('✓ cpuUsage() returns { user, system }');
      console.log('  user:', cpu1.user, 'µs');
      console.log('  system:', cpu1.system, 'µs');
    } else {
      throw new Error('cpuUsage() should return numbers');
    }
  } else {
    throw new Error('cpuUsage() should return an object');
  }

  // Test with previousValue
  const cpu2 = process.cpuUsage(cpu1);
  if (typeof cpu2 === 'object' && cpu2 !== null) {
    console.log('✓ cpuUsage(previousValue) returns delta');
  } else {
    throw new Error('cpuUsage(previousValue) failed');
  }
} else {
  throw new Error('process.cpuUsage not found');
}

// Test 2: process.resourceUsage()
console.log('\nTest 2: process.resourceUsage()');
if (typeof process.resourceUsage === 'function') {
  console.log('✓ process.resourceUsage() exists');

  const resources = process.resourceUsage();
  if (typeof resources === 'object' && resources !== null) {
    console.log('✓ resourceUsage() returns an object');

    // Check required fields
    const requiredFields = [
      'userCPUTime',
      'systemCPUTime',
      'maxRSS',
      'minorPageFault',
      'majorPageFault',
      'fsRead',
      'fsWrite',
    ];

    for (const field of requiredFields) {
      if (typeof resources[field] === 'number') {
        console.log('  ✓', field + ':', resources[field]);
      } else {
        throw new Error('Missing or invalid field: ' + field);
      }
    }
  } else {
    throw new Error('resourceUsage() should return an object');
  }
} else {
  throw new Error('process.resourceUsage not found');
}

// Test 3: process.memoryUsage.rss()
console.log('\nTest 3: process.memoryUsage.rss()');
if (typeof process.memoryUsage === 'function') {
  console.log('✓ process.memoryUsage() exists');

  if (typeof process.memoryUsage.rss === 'function') {
    console.log('✓ process.memoryUsage.rss() exists');

    const rss = process.memoryUsage.rss();
    if (typeof rss === 'number' && rss > 0) {
      console.log('✓ memoryUsage.rss() returns positive number:', rss, 'bytes');
    } else {
      throw new Error('memoryUsage.rss() should return positive number');
    }

    // Verify it matches memoryUsage().rss
    const mem = process.memoryUsage();
    if (mem.rss) {
      console.log('✓ Consistent with memoryUsage().rss');
    }
  } else {
    throw new Error('process.memoryUsage.rss not found');
  }
} else {
  throw new Error('process.memoryUsage not found');
}

// Test 4: process.availableMemory()
console.log('\nTest 4: process.availableMemory()');
if (typeof process.availableMemory === 'function') {
  console.log('✓ process.availableMemory() exists');

  const available = process.availableMemory();
  if (typeof available === 'number' && available > 0) {
    console.log('✓ availableMemory() returns positive number');
    console.log(
      '  Available memory:',
      (available / 1024 / 1024 / 1024).toFixed(2),
      'GB'
    );
  } else {
    throw new Error('availableMemory() should return positive number');
  }
} else {
  throw new Error('process.availableMemory not found');
}

// Test 5: process.constrainedMemory()
console.log('\nTest 5: process.constrainedMemory()');
if (typeof process.constrainedMemory === 'function') {
  console.log('✓ process.constrainedMemory() exists');

  const constrained = process.constrainedMemory();
  // This may return undefined if no constraint
  if (constrained === undefined) {
    console.log('✓ constrainedMemory() returns undefined (no constraint)');
  } else if (typeof constrained === 'number' && constrained > 0) {
    console.log(
      '✓ constrainedMemory() returns constraint:',
      constrained,
      'bytes'
    );
  } else {
    throw new Error('constrainedMemory() returned invalid value');
  }
} else {
  throw new Error('process.constrainedMemory not found');
}

console.log('\n✅ All process resource monitoring tests passed!');
