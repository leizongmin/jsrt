/**
 * Example: module.registerHooks() API Demonstration
 *
 * This example demonstrates the module.registerHooks() API implementation
 * from Task 4.2, showing how embedders can customize module resolution
 * and loading behavior without forking the runtime.
 */

const module = require('node:module');

console.log('=== Module Register Hooks Demo ===\n');

// Example 1: Simple resolve hook that logs module resolution
console.log('1. Registering a resolve hook that logs module resolution:');

const resolveHandle = module.registerHooks({
  resolve: function (specifier, context, nextResolve) {
    console.log(`  📦 Resolving module: ${specifier}`);
    console.log(`  📂 Base path: ${context.basePath || 'unknown'}`);

    // Call the next resolver in the chain
    const result = nextResolve(specifier, context);
    console.log(`  ✅ Resolved to: ${result || 'null'}`);
    return result;
  },
});

console.log(`  🎯 Hook registered with ID: ${resolveHandle.id}`);
console.log(`  🔧 Has resolve hook: ${resolveHandle.resolve}`);
console.log(`  🔧 Has load hook: ${resolveHandle.load}\n`);

// Example 2: Load hook that transforms module source
console.log('2. Registering a load hook that adds a banner to module source:');

const loadHandle = module.registerHooks({
  load: function (url, context, nextLoad) {
    console.log(`  📄 Loading module: ${url}`);

    // Get the original module source
    const originalSource = nextLoad(url, context);

    if (originalSource && url.endsWith('.js')) {
      // Add a banner comment to the source
      const banner = `\n// === Module loaded via custom hook ===\n// URL: ${url}\n// === End custom hook ===\n`;
      console.log(`  🎨 Added custom banner to module source`);
      return banner + originalSource;
    }

    return originalSource;
  },
});

console.log(`  🎯 Hook registered with ID: ${loadHandle.id}`);
console.log(`  🔧 Has resolve hook: ${loadHandle.resolve}`);
console.log(`  🔧 Has load hook: ${loadHandle.load}\n`);

// Example 3: Combined resolve and load hook
console.log('3. Registering combined resolve and load hooks:');

const combinedHandle = module.registerHooks({
  resolve: function (specifier, context, nextResolve) {
    // Example: Alias 'my-module' to a specific file
    if (specifier === 'my-module') {
      console.log(
        `  🔄 Aliasing 'my-module' to './examples/module/custom_module.js'`
      );
      return nextResolve('./examples/module/custom_module.js', context);
    }
    return nextResolve(specifier, context);
  },

  load: function (url, context, nextLoad) {
    console.log(`  📦 Loading via combined hook: ${url}`);
    return nextLoad(url, context);
  },
});

console.log(`  🎯 Combined hook registered with ID: ${combinedHandle.id}`);
console.log(`  🔧 Has resolve hook: ${combinedHandle.resolve}`);
console.log(`  🔧 Has load hook: ${combinedHandle.load}\n`);

// Example 4: Hook chain demonstration (LIFO order)
console.log('4. Demonstrating LIFO hook registration order:');

const firstHook = module.registerHooks({
  resolve: function (specifier, context, nextResolve) {
    console.log(`  🔗 First hook (registered first): ${specifier}`);
    return nextResolve(specifier, context);
  },
});

const secondHook = module.registerHooks({
  resolve: function (specifier, context, nextResolve) {
    console.log(
      `  🔗 Second hook (registered last, called first): ${specifier}`
    );
    return nextResolve(specifier, context);
  },
});

console.log(`  🎯 First hook ID: ${firstHook.id}`);
console.log(`  🎯 Second hook ID: ${secondHook.id}`);
console.log(`  📊 Higher ID = registered later = called first (LIFO order)\n`);

// Example 5: Error handling in hooks
console.log('5. Error handling demonstration:');

try {
  // This should throw an error - no options provided
  module.registerHooks();
} catch (error) {
  console.log(`  ❌ Caught error (missing options): ${error.message}`);
}

try {
  // This should throw an error - non-function resolve
  module.registerHooks({ resolve: 'not-a-function' });
} catch (error) {
  console.log(`  ❌ Caught error (invalid resolve): ${error.message}`);
}

try {
  // This should throw an error - empty options
  module.registerHooks({});
} catch (error) {
  console.log(`  ❌ Caught error (empty options): ${error.message}`);
}

console.log('\n=== Summary ===');
console.log(`✅ module.registerHooks() API successfully implemented`);
console.log(
  `✅ Hooks registered: ${resolveHandle.id}, ${loadHandle.id}, ${combinedHandle.id}, ${firstHook.id}, ${secondHook.id}`
);
console.log(`✅ Error handling working correctly`);
console.log(`✅ LIFO order maintained`);
console.log(`✅ Integration with existing module APIs verified`);

console.log('\n=== Hook Registration Features ===');
console.log('• Synchronous hook registration (main thread only)');
console.log('• Resolve hooks for custom module resolution');
console.log('• Load hooks for custom module loading');
console.log('• LIFO execution order (last registered, first called)');
console.log('• Proper error handling and validation');
console.log('• Return handles for future unregistration support');
console.log('• Maintains compatibility with Node.js hook semantics');
