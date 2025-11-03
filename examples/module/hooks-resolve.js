#!/usr/bin/env jsrt

/**
 * Example: Module Resolve Hook
 *
 * This example demonstrates how to use resolve hooks to customize
 * module resolution behavior.
 *
 * Resolve hooks allow you to:
 * - Intercept module resolution requests
 * - Rewrite module specifiers
 * - Implement custom module resolution algorithms
 * - Add virtual module support
 */

const { registerHooks } = require('node:module');

console.log('=== Module Resolve Hook Example ===\n');

// Example 1: Simple module remapping
console.log('1. Remapping module names:');
registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log(`   Resolving: ${specifier}`);
    console.log(`   Parent: ${context.parentPath || '(main)'}`);

    // Example: Map 'my-custom-lib' to a specific version
    if (specifier === 'my-custom-lib') {
      console.log('   -> Remapping to specific version');
      return {
        url: './vendor/my-custom-lib-v2.0.0.js',
        format: 'commonjs',
        shortCircuit: true,
      };
    }

    // Example: Implement 'virtual:' protocol
    if (specifier.startsWith('virtual:')) {
      console.log('   -> Creating virtual module');
      return {
        url: specifier,
        format: 'commonjs',
        shortCircuit: true,
      };
    }

    // Default: call next hook or use default resolution
    const result = nextResolve();

    if (result) {
      console.log(`   -> Resolved to: ${result.url || result}`);
    }

    return result;
  },
});

// Trigger resolve hook
try {
  require('my-custom-lib');
} catch (e) {
  console.log(`   Expected error: ${e.message}\n`);
}

console.log('2. Testing multiple hooks (LIFO order):\n');

// Example 2: Multiple hooks in LIFO order
const hook1 = registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log('   Hook 1 (outer) - called first');
    const result = nextResolve();
    console.log('   Hook 1 (outer) - after nextResolve');
    return result;
  },
});

const hook2 = registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log('   Hook 2 (inner) - called second');
    const result = nextResolve();
    console.log('   Hook 2 (inner) - after nextResolve');
    return result;
  },
});

// This will show LIFO order (hook2 called before hook1)
try {
  require('fs');
  console.log('   Successfully required fs\n');
} catch (e) {
  console.log(`   Error: ${e.message}\n`);
}

console.log('3. Resolving custom protocol:\n');

// Example 3: Custom protocol resolution
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('app:')) {
      // Custom protocol
      const modulePath = specifier.replace('app:', './src/');
      console.log(`   Custom protocol 'app:' -> ${modulePath}`);
      return {
        url: modulePath,
        format: 'commonjs',
        shortCircuit: true,
      };
    }

    return nextResolve();
  },
});

try {
  // This would work if app:module existed
  console.log('   Attempting to resolve app:module...');
  // const appModule = require('app:module');
} catch (e) {
  console.log(`   Expected error: ${e.message}\n`);
}

console.log('\n=== Resolve Hook Examples Complete ===');
console.log('\nKey Takeaways:');
console.log('- Resolve hooks run LIFO (last registered, first called)');
console.log('- Use nextResolve() to call the next hook in chain');
console.log('- Return { url, format, shortCircuit } to override resolution');
console.log('- Return undefined/null to use default resolution');
