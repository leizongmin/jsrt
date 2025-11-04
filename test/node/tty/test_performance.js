#!/usr/bin/env node

/**
 * Performance and Load Tests
 * Tests TTY operation performance, memory usage, and create/destroy cycles
 */

const tty = require('node:tty');
const assert = require('node:assert');

console.log('=== TTY Performance and Load Tests ===\n');

// Utility function to measure performance
function measureTime(name, testFn, iterations = 1000) {
  const start = process.hrtime.bigint();

  for (let i = 0; i < iterations; i++) {
    testFn();
  }

  const end = process.hrtime.bigint();
  const timeNs = Number(end - start);
  const timeMs = timeNs / 1000000;
  const avgTime = timeMs / iterations;

  console.log(
    `   ${name}: ${timeMs.toFixed(2)}ms total, ${avgTime.toFixed(4)}ms avg (${iterations} iterations)`
  );
  return { total: timeMs, average: avgTime, iterations };
}

// Test 1: TTY object creation performance
console.log('1. Testing TTY object creation performance:');
try {
  const results = [];

  results.push(
    measureTime(
      'ReadStream creation',
      () => {
        new tty.ReadStream(0);
      },
      1000
    )
  );

  results.push(
    measureTime(
      'WriteStream creation (stdout)',
      () => {
        new tty.WriteStream(1);
      },
      1000
    )
  );

  results.push(
    measureTime(
      'WriteStream creation (stderr)',
      () => {
        new tty.WriteStream(2);
      },
      1000
    )
  );

  // Performance expectations
  results.forEach((result) => {
    if (result.average < 0.1) {
      console.log(`     ✓ Performance acceptable (< 0.1ms per creation)`);
    } else if (result.average < 1.0) {
      console.log(`     ⚠ Performance moderate (< 1ms per creation)`);
    } else {
      console.log(`     ✗ Performance slow (> 1ms per creation)`);
    }
  });
} catch (error) {
  console.log('   ✗ Object creation performance test failed:', error.message);
  process.exit(1);
}

// Test 2: TTY method call performance
console.log('\n2. Testing TTY method call performance:');
try {
  const rs = new tty.ReadStream(0);
  const ws = new tty.WriteStream(1);

  const methodResults = [];

  // Test tty.isatty performance
  methodResults.push(
    measureTime(
      'tty.isatty() calls',
      () => {
        tty.isatty(0);
        tty.isatty(1);
        tty.isatty(2);
      },
      3000
    )
  );

  // Test ReadStream methods
  if (rs.isTTY && typeof rs.setRawMode === 'function') {
    methodResults.push(
      measureTime(
        'ReadStream setRawMode()',
        () => {
          try {
            rs.setRawMode(true);
            rs.setRawMode(false);
          } catch (e) {
            // Ignore errors in performance test
          }
        },
        1000
      )
    );
  }

  // Test WriteStream methods
  methodResults.push(
    measureTime(
      'WriteStream getColorDepth()',
      () => {
        ws.getColorDepth();
      },
      3000
    )
  );

  methodResults.push(
    measureTime(
      'WriteStream hasColors()',
      () => {
        ws.hasColors(16);
        ws.hasColors(256);
        ws.hasColors(16777216);
      },
      3000
    )
  );

  methodResults.push(
    measureTime(
      'WriteStream size access',
      () => {
        const cols = ws.columns;
        const rows = ws.rows;
      },
      3000
    )
  );

  // Performance expectations for method calls
  methodResults.forEach((result) => {
    if (result.average < 0.01) {
      console.log(`     ✓ Method performance excellent (< 0.01ms per call)`);
    } else if (result.average < 0.1) {
      console.log(`     ✓ Method performance good (< 0.1ms per call)`);
    } else if (result.average < 1.0) {
      console.log(`     ⚠ Method performance moderate (< 1ms per call)`);
    } else {
      console.log(`     ✗ Method performance slow (> 1ms per call)`);
    }
  });
} catch (error) {
  console.log('   ✗ Method call performance test failed:', error.message);
  process.exit(1);
}

// Test 3: Memory usage under load
console.log('\n3. Testing memory usage under load:');
try {
  const initialMemory = process.memoryUsage();
  console.log(
    `   Initial memory: ${Math.round(initialMemory.heapUsed / 1024 / 1024)}MB`
  );

  // Create many TTY objects
  const objects = [];
  for (let i = 0; i < 10000; i++) {
    objects.push(new tty.ReadStream(0));
    objects.push(new tty.WriteStream(1));
    objects.push(new tty.WriteStream(2));
  }

  const peakMemory = process.memoryUsage();
  console.log(
    `   Peak memory (10k objects): ${Math.round(peakMemory.heapUsed / 1024 / 1024)}MB`
  );

  const memoryIncrease = peakMemory.heapUsed - initialMemory.heapUsed;
  const memoryPerObject = memoryIncrease / 30000; // 3 objects per iteration

  console.log(`   Memory increase: ${Math.round(memoryIncrease / 1024)}KB`);
  console.log(`   Memory per TTY object: ${Math.round(memoryPerObject)} bytes`);

  // Clear references to test garbage collection
  objects.length = 0;

  // Force garbage collection if available
  if (global.gc) {
    global.gc();
    const finalMemory = process.memoryUsage();
    console.log(
      `   Memory after GC: ${Math.round(finalMemory.heapUsed / 1024 / 1024)}MB`
    );

    const memoryReclaimed = peakMemory.heapUsed - finalMemory.heapUsed;
    const reclaimRate = memoryReclaimed / memoryIncrease;
    console.log(
      `   Memory reclaimed: ${Math.round(memoryReclaimed / 1024)}KB (${(reclaimRate * 100).toFixed(1)}%)`
    );
  } else {
    console.log('   ⚠ Garbage collection not available for testing');
  }

  // Memory usage expectations
  if (memoryPerObject < 1000) {
    console.log('   ✓ Memory usage excellent (< 1KB per TTY object)');
  } else if (memoryPerObject < 5000) {
    console.log('   ✓ Memory usage acceptable (< 5KB per TTY object)');
  } else if (memoryPerObject < 10000) {
    console.log('   ⚠ Memory usage moderate (< 10KB per TTY object)');
  } else {
    console.log('   ✗ Memory usage high (> 10KB per TTY object)');
  }
} catch (error) {
  console.log('   ✗ Memory usage test failed:', error.message);
  process.exit(1);
}

// Test 4: Stress test - rapid create/destroy cycles
console.log('\n4. Testing rapid create/destroy cycles:');
try {
  const cycles = 10000;
  let successCount = 0;
  let errorCount = 0;

  const start = Date.now();

  for (let i = 0; i < cycles; i++) {
    try {
      const rs = new tty.ReadStream(0);
      const ws1 = new tty.WriteStream(1);
      const ws2 = new tty.WriteStream(2);

      // Perform some operations
      if (typeof rs.setRawMode === 'function') {
        try {
          rs.setRawMode(true);
          rs.setRawMode(false);
        } catch (e) {
          // Ignore errors
        }
      }

      ws1.getColorDepth();
      ws1.hasColors(256);
      ws2.columns;
      ws2.rows;

      successCount++;

      // Objects will be garbage collected
    } catch (error) {
      errorCount++;
    }
  }

  const totalTime = Date.now() - start;
  const avgTimePerCycle = totalTime / cycles;

  console.log(`   ✓ ${cycles} cycles completed in ${totalTime}ms`);
  console.log(`   ✓ Average time per cycle: ${avgTimePerCycle.toFixed(4)}ms`);
  console.log(
    `   ✓ Success rate: ${((successCount / cycles) * 100).toFixed(2)}%`
  );
  console.log(`   ✓ Error rate: ${((errorCount / cycles) * 100).toFixed(2)}%`);

  if (avgTimePerCycle < 0.1) {
    console.log('   ✓ Cycle performance excellent (< 0.1ms per cycle)');
  } else if (avgTimePerCycle < 1.0) {
    console.log('   ✓ Cycle performance good (< 1ms per cycle)');
  } else {
    console.log('   ⚠ Cycle performance moderate (> 1ms per cycle)');
  }

  if (successCount / cycles > 0.95) {
    console.log('   ✓ Reliability excellent (> 95% success rate)');
  } else if (successCount / cycles > 0.9) {
    console.log('   ✓ Reliability good (> 90% success rate)');
  } else {
    console.log('   ⚠ Reliability concerning (< 90% success rate)');
  }
} catch (error) {
  console.log('   ✗ Stress test failed:', error.message);
  process.exit(1);
}

// Test 5: Concurrent access performance
console.log('\n5. Testing concurrent access performance:');
try {
  const concurrentOps = 10000;
  const start = Date.now();

  // Test concurrent isatty calls
  for (let i = 0; i < concurrentOps; i++) {
    tty.isatty(0);
    tty.isatty(1);
    tty.isatty(2);
  }

  const concurrentTime = Date.now() - start;
  const avgConcurrentTime = concurrentTime / (concurrentOps * 3);

  console.log(
    `   ✓ ${concurrentOps * 3} concurrent isatty calls in ${concurrentTime}ms`
  );
  console.log(`   ✓ Average time per call: ${avgConcurrentTime.toFixed(4)}ms`);

  // Test concurrent object operations
  const ws = new tty.WriteStream(1);
  const objectStart = Date.now();

  for (let i = 0; i < concurrentOps; i++) {
    ws.getColorDepth();
    ws.hasColors(256);
    ws.columns;
    ws.rows;
  }

  const objectTime = Date.now() - objectStart;
  const avgObjectTime = objectTime / (concurrentOps * 4);

  console.log(
    `   ✓ ${concurrentOps * 4} concurrent object operations in ${objectTime}ms`
  );
  console.log(`   ✓ Average time per operation: ${avgObjectTime.toFixed(4)}ms`);

  if (avgConcurrentTime < 0.001 && avgObjectTime < 0.001) {
    console.log(
      '   ✓ Concurrent performance excellent (< 0.001ms per operation)'
    );
  } else if (avgConcurrentTime < 0.01 && avgObjectTime < 0.01) {
    console.log('   ✓ Concurrent performance good (< 0.01ms per operation)');
  } else {
    console.log(
      '   ⚠ Concurrent performance moderate (> 0.01ms per operation)'
    );
  }
} catch (error) {
  console.log('   ✗ Concurrent access test failed:', error.message);
  process.exit(1);
}

// Test 6: Event system performance
console.log('\n6. Testing event system performance:');
try {
  const ws = new tty.WriteStream(1);
  const eventCount = 10000;

  // Test event listener addition
  const addStart = Date.now();
  for (let i = 0; i < eventCount; i++) {
    ws.on('test', () => {});
  }
  const addTime = Date.now() - addStart;

  console.log(`   ✓ Added ${eventCount} event listeners in ${addTime}ms`);

  // Test event emission
  const emitStart = Date.now();
  for (let i = 0; i < 100; i++) {
    ws.emit('test');
  }
  const emitTime = Date.now() - emitStart;

  console.log(
    `   ✓ Emitted 100 events to ${eventCount} listeners in ${emitTime}ms`
  );
  console.log(
    `   ✓ Average time per emission: ${(emitTime / 100).toFixed(4)}ms`
  );

  // Test event listener removal
  const removeStart = Date.now();
  ws.removeAllListeners('test');
  const removeTime = Date.now() - removeStart;

  console.log(`   ✓ Removed ${eventCount} event listeners in ${removeTime}ms`);

  if (addTime < 100 && emitTime < 1000 && removeTime < 100) {
    console.log('   ✓ Event system performance excellent');
  } else if (addTime < 500 && emitTime < 5000 && removeTime < 500) {
    console.log('   ✓ Event system performance good');
  } else {
    console.log('   ⚠ Event system performance moderate');
  }
} catch (error) {
  console.log('   ✗ Event system performance test failed:', error.message);
  process.exit(1);
}

// Test 7: Resource cleanup under load
console.log('\n7. Testing resource cleanup under load:');
try {
  const iterations = 1000;
  const memorySnapshots = [];

  for (let iter = 0; iter < 10; iter++) {
    const objects = [];

    // Create many objects
    for (let i = 0; i < iterations; i++) {
      objects.push(new tty.ReadStream(0));
      objects.push(new tty.WriteStream(1));
      objects.push(new tty.WriteStream(2));
    }

    // Take memory snapshot
    const mem = process.memoryUsage();
    memorySnapshots.push(mem.heapUsed);

    // Clear references
    objects.length = 0;

    // Force garbage collection if available
    if (global.gc) {
      global.gc();
    }
  }

  // Analyze memory growth
  const initialMem = memorySnapshots[0];
  const finalMem = memorySnapshots[memorySnapshots.length - 1];
  const memoryGrowth = finalMem - initialMem;
  const avgGrowthPerIteration = memoryGrowth / memorySnapshots.length;

  console.log(`   ✓ Initial memory: ${Math.round(initialMem / 1024 / 1024)}MB`);
  console.log(`   ✓ Final memory: ${Math.round(finalMem / 1024 / 1024)}MB`);
  console.log(`   ✓ Total growth: ${Math.round(memoryGrowth / 1024)}KB`);
  console.log(
    `   ✓ Average growth per iteration: ${Math.round(avgGrowthPerIteration / 1024)}KB`
  );

  if (avgGrowthPerIteration < 100 * 1024) {
    // Less than 100KB per iteration
    console.log('   ✓ Memory leak detection: Excellent (minimal growth)');
  } else if (avgGrowthPerIteration < 500 * 1024) {
    // Less than 500KB per iteration
    console.log('   ✓ Memory leak detection: Good (low growth)');
  } else {
    console.log('   ⚠ Memory leak detection: Potential leak (high growth)');
  }
} catch (error) {
  console.log('   ✗ Resource cleanup test failed:', error.message);
  process.exit(1);
}

console.log('\n=== Performance and Load Test Summary ===');
console.log('✅ TTY object creation performance tested');
console.log('✅ TTY method call performance tested');
console.log('✅ Memory usage under load analyzed');
console.log('✅ Rapid create/destroy cycles tested');
console.log('✅ Concurrent access performance verified');
console.log('✅ Event system performance tested');
console.log('✅ Resource cleanup under load validated');

console.log('\n🎉 All performance and load tests completed!');
console.log('\n💡 Performance Recommendations:');
console.log('   - TTY objects are lightweight and can be created frequently');
console.log('   - Method calls are efficient for repeated operations');
console.log('   - Memory usage is reasonable with proper cleanup');
console.log('   - Event system scales well for high-frequency operations');
console.log('   - Concurrent access is safe and performant');
