#!/usr/bin/env jsrt

/**
 * Example: Module Load Hook
 *
 * This example demonstrates how to use load hooks to customize
 * module loading behavior.
 *
 * Load hooks allow you to:
 * - Intercept module loading requests
 * - Transform module source code
 * - Implement virtual modules
 * - Add custom module formats
 */

const { registerHooks } = require('node:module');

console.log('=== Module Load Hook Example ===\n');

// Example 1: Source code transformation
console.log('1. Transforming module source:\n');

registerHooks({
  load(url, context, nextLoad) {
    console.log(`   Loading: ${url}`);
    console.log(`   Format: ${context.format}`);

    // Example: Inline source transformation
    if (url.includes('transform-me')) {
      console.log('   -> Applying source transformation');

      return {
        format: 'commonjs',
        source: `
          // Transformed by load hook
          console.log('[Transformed Module] Loading...');
          const original = 'Hello World';
          module.exports = {
            message: original + ' (transformed!)',
            transformed: true
          };
        `,
        shortCircuit: true,
      };
    }

    // Default: call next hook or use default loading
    return nextLoad();
  },
});

// Create a test file to load
const fs = require('fs');
const path = './tmp-transform-test.js';
fs.writeFileSync(path, 'module.exports = "original";');

try {
  const transformed = require(path);
  console.log(`   Result: ${JSON.stringify(transformed)}\n`);
} catch (e) {
  console.log(`   Error: ${e.message}\n`);
}

console.log('2. Virtual module creation:\n');

// Example 2: Create virtual modules
registerHooks({
  load(url, context, nextLoad) {
    if (url.startsWith('virtual:')) {
      const moduleName = url.replace('virtual:', '');
      console.log(`   Creating virtual module: ${moduleName}`);

      return {
        format: 'commonjs',
        source: `
          console.log('[Virtual Module] ${moduleName} loaded');
          module.exports = {
            name: '${moduleName}',
            data: [1, 2, 3],
            timestamp: Date.now()
          };
        `,
        shortCircuit: true,
      };
    }

    return nextLoad();
  },
});

try {
  // Virtual module
  console.log('   Requiring virtual:greeting...');
  const virtualModule = require('virtual:greeting');
  console.log(`   Module exports: ${JSON.stringify(virtualModule)}\n`);
} catch (e) {
  console.log(`   Error: ${e.message}\n`);
}

console.log('3. Format conversion (ESM to CommonJS):\n');

// Example 3: Format conversion
registerHooks({
  load(url, context, nextLoad) {
    if (url.includes('.mjs')) {
      console.log('   Converting ESM to CommonJS');

      return nextLoad().then((result) => {
        // In a real scenario, you'd parse and convert ESM to CJS
        return {
          format: 'commonjs',
          source: `
            // Converted from ESM
            exports.__esModule = true;
            exports.default = 42;
            exports.named = 'value';
          `,
          shortCircuit: true,
        };
      });
    }

    return nextLoad();
  },
});

console.log('\n4. Multiple load hooks (LIFO order):\n');

// Example 4: Multiple hooks
const hook1 = registerHooks({
  load(url, context, nextLoad) {
    console.log('   Load Hook 1 (outer)');
    const result = nextLoad();
    console.log('   Load Hook 1 (outer) - after nextLoad');
    return result;
  },
});

const hook2 = registerHooks({
  load(url, context, nextLoad) {
    console.log('   Load Hook 2 (inner)');
    const result = nextLoad();
    console.log('   Load Hook 2 (inner) - after nextLoad');
    return result;
  },
});

try {
  require('fs');
  console.log('   Successfully loaded fs\n');
} catch (e) {
  console.log(`   Error: ${e.message}\n`);
}

console.log('5. Conditional loading based on context:\n');

// Example 5: Conditional loading
registerHooks({
  load(url, context, nextLoad) {
    // Add debug logging for certain conditions
    if (context.conditions && context.conditions.includes('development')) {
      console.log('   Development mode: adding debug wrapper');

      return nextLoad().then((result) => {
        return {
          format: result.format || 'commonjs',
          source: `
            console.log('[Debug] Loading:', '${url}');
            ${result.source}
            console.log('[Debug] Loaded:', '${url}');
          `,
          shortCircuit: true,
        };
      });
    }

    return nextLoad();
  },
});

console.log('\n=== Load Hook Examples Complete ===');
console.log('\nKey Takeaways:');
console.log('- Load hooks can transform or replace module source');
console.log('- Return { format, source, shortCircuit } to override loading');
console.log('- Use nextLoad() to call the next hook in chain');
console.log(
  '- Support both string and binary (ArrayBuffer, Uint8Array) sources'
);
console.log('- Can implement virtual modules and format conversion');
