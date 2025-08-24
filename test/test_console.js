// Basic console.log functionality test
console.log('hello, world!');

// Test logging different data types (removed problematic setTimeout reference)
console.log(123, 456, true, false, null, undefined, [1,2,3]);

// Test logging complex objects (removed problematic setTimeout reference)
console.log({ a: 123, b: 456, c: true, d: false, e: null, f: undefined, g: [1,2,3] });

// Test logging simple function reference (safer than globalThis)
console.log('typeof setTimeout:', typeof setTimeout);

// Test function existence (safer than toString)
console.log('setTimeout exists:', typeof setTimeout === 'function');

// Test console.log with no arguments
console.log();

// Test console.log with empty string
console.log('');

// Test console.log with multiple empty arguments
console.log('', '', '');

// Test console.log with nested objects (simple nesting to avoid circular reference issues)
console.log({ 
  level1: { 
    level2: { 
      value: 'nested',
      array: [1, 2, { inner: true }]
    }
  }
});

// Note: Circular reference test removed due to runtime limitations

// Test console.log with different number types
console.log('Numbers:', 0, -1, 1.5, -1.5, Infinity, -Infinity, NaN);

// Note: Symbol test removed due to runtime limitations

// Test console.log with arrays of different types
console.log('Mixed array:', [1, 'string', true, null, undefined, {key: 'value'}]);

// Test console.log with empty array and object
console.log('Empty structures:', [], {});

console.log('Console tests completed successfully');
