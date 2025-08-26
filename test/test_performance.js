// Performance API tests for WinterCG compliance
console.log('=== Starting Performance API Tests ===');

// Test 1: Check if performance object exists
console.log('Test 1: Performance object availability');
if (typeof performance === 'object' && performance !== null) {
  console.log('✅ PASS: performance object is available');
} else {
  console.log('❌ FAIL: performance object is not available');
}

// Test 2: Check performance.now() method
console.log('Test 2: performance.now() method');
if (typeof performance.now === 'function') {
  console.log('✅ PASS: performance.now is a function');
  
  // Test basic functionality
  const start = performance.now();
  console.log('performance.now() at start =', start);
  
  if (typeof start === 'number' && start >= 0) {
    console.log('✅ PASS: performance.now() returns a non-negative number');
  } else {
    console.log('❌ FAIL: performance.now() should return a non-negative number, got:', start);
  }
} else {
  console.log('❌ FAIL: performance.now is not a function');
}

// Test 3: Check performance.timeOrigin property
console.log('Test 3: performance.timeOrigin property');
if ('timeOrigin' in performance) {
  const timeOrigin = performance.timeOrigin;
  console.log('performance.timeOrigin =', timeOrigin);
  
  if (typeof timeOrigin === 'number') {
    console.log('✅ PASS: performance.timeOrigin is a number');
  } else {
    console.log('❌ FAIL: performance.timeOrigin should be a number, got:', typeof timeOrigin);
  }
} else {
  console.log('❌ FAIL: performance.timeOrigin property is not available');
}

// Test 4: Monotonic time progression
console.log('Test 4: Monotonic time progression');
if (typeof performance.now === 'function') {
  const time1 = performance.now();
  
  // Small delay (busy wait)
  for (let i = 0; i < 10000; i++) {
    Math.random();
  }
  
  const time2 = performance.now();
  
  console.log('Time 1:', time1);
  console.log('Time 2:', time2);
  console.log('Difference:', time2 - time1);
  
  if (time2 >= time1) {
    console.log('✅ PASS: Time progresses monotonically');
  } else {
    console.log('❌ FAIL: Time should not go backwards');
  }
  
  if (time2 - time1 > 0) {
    console.log('✅ PASS: Time difference is positive (some time elapsed)');
  } else {
    console.log('ℹ️ INFO: Very little time elapsed (< 1ms resolution)');
  }
}

// Test 5: High precision timing
console.log('Test 5: High precision timing');
if (typeof performance.now === 'function') {
  const measurements = [];
  
  for (let i = 0; i < 5; i++) {
    measurements.push(performance.now());
  }
  
  console.log('Measurements:', measurements);
  
  // Check if we get different values (indicating high precision)
  let allSame = true;
  for (let i = 1; i < measurements.length; i++) {
    if (measurements[i] !== measurements[0]) {
      allSame = false;
      break;
    }
  }
  
  if (!allSame) {
    console.log('✅ PASS: performance.now() provides varying precision');
  } else {
    console.log('ℹ️ INFO: All measurements were identical (very fast execution)');
  }
}

// Test 6: Timer resolution test
console.log('Test 6: Timer resolution capability');
if (typeof performance.now === 'function') {
  const start = performance.now();
  
  // Use setTimeout to create a measurable delay
  setTimeout(() => {
    const end = performance.now();
    const elapsed = end - start;
    
    console.log('Elapsed time with setTimeout(0):', elapsed, 'ms');
    
    if (elapsed >= 0) {
      console.log('✅ PASS: Can measure setTimeout intervals');
    } else {
      console.log('❌ FAIL: Negative elapsed time detected');
    }
    
    // Test 7: Comparison with console.time
    console.log('Test 7: Comparison with console timing');
    
    if (typeof console.time === 'function' && typeof console.timeEnd === 'function') {
      console.time('performance-comparison');
      const perfStart = performance.now();
      
      // Small computation
      let sum = 0;
      for (let i = 0; i < 100000; i++) {
        sum += Math.sqrt(i);
      }
      
      const perfEnd = performance.now();
      console.timeEnd('performance-comparison');
      
      const perfElapsed = perfEnd - perfStart;
      console.log('performance.now() measured:', perfElapsed, 'ms');
      console.log('Sum result:', sum); // Prevent optimization
      
      if (perfElapsed > 0) {
        console.log('✅ PASS: performance.now() measured positive elapsed time');
      } else {
        console.log('ℹ️ INFO: Operation completed in less than timer resolution');
      }
    }
    
    console.log('=== Performance API Tests Completed ===');
  }, 10);
}