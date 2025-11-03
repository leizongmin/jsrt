# Module Hooks API

## Overview

The `node:module` hook system allows you to customize module resolution and loading behavior without modifying the runtime. This enables powerful use cases such as:

- Custom module resolution algorithms
- Source code transformation
- Virtual module implementation
- Development/production environment customization
- Language compilation (TypeScript, etc.)
- Mocking and testing

## Quick Start

```javascript
const { registerHooks } = require('node:module');

// Register hooks
registerHooks({
  resolve(specifier, context, nextResolve) {
    // Customize resolution
    return nextResolve();
  },

  load(url, context, nextLoad) {
    // Customize loading
    return nextLoad();
  },
});
```

## API Reference

### `registerHooks(options)`

Registers module hooks that will be called during module resolution and loading.

#### Parameters

- `options` (Object): Hook configuration
  - `resolve` (Function, optional): Resolve hook function
  - `load` (Function, optional): Load hook function

#### Returns

`undefined` - hooks are added to the registry (currently no unregistration API)

#### Examples

```javascript
// Register only a resolve hook
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('my-lib:')) {
      return {
        url: './vendor/' + specifier.replace('my-lib:', ''),
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve();
  },
});

// Register both hooks
registerHooks({
  resolve(specifier, context, nextResolve) {
    return nextResolve();
  },
  load(url, context, nextLoad) {
    return nextLoad();
  },
});
```

## Resolve Hook

### Signature

```typescript
resolve(
  specifier: string,
  context: ResolveContext,
  nextResolve: (specifier?: string) => ResolveResult | null
): ResolveResult | null | void
```

### Parameters

#### `specifier`

The module specifier being resolved (e.g., `'lodash'`, `'./module.js'`, `'node:fs'`)

#### `context`

An object containing resolution context:

```typescript
interface ResolveContext {
  specifier: string;        // Original specifier
  parentPath?: string;      // Path of parent module (if applicable)
  resolvedUrl?: string;     // Resolved URL (if available)
  isMain: boolean;          // Whether this is the main module
  conditions?: string[];    // Resolution conditions (e.g., ['node', 'import'])
}
```

#### `nextResolve`

Function to call the next hook in the chain, or to use default resolution.

**Usage:**
- Call `nextResolve()` to continue with default resolution
- Call `nextResolve(customSpecifier)` to resolve a different specifier
- Return the result directly to short-circuit the hook chain

### Return Value

Return one of the following:

1. **Custom resolution result:**
   ```typescript
   {
     url: string;                    // Resolved module URL
     format?: string;               // 'commonjs' | 'module' | 'json' | etc.
     shortCircuit?: boolean;        // Skip remaining hooks (default: true)
   }
   ```

2. **Short-circuit with URL only:**
   ```typescript
   'file:///path/to/module.js'  // Legacy format, equivalent to { url, shortCircuit: true }
   ```

3. **Continue processing:**
   ```typescript
   null    // Let next hook handle it
   undefined  // Same as null
   nextResolve()  // Call next hook
   ```

### Examples

```javascript
// Simple module remapping
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'old-lib') {
      return {
        url: 'node_modules/new-lib/index.js',
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve();
  },
});

// Custom protocol
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.startsWith('app:')) {
      return {
        url: `./src/${specifier.replace('app:', '')}`,
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve();
  },
});

// Multiple hooks in chain
registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log('Hook 1');
    const result = nextResolve();
    console.log('Hook 1 - after nextResolve');
    return result;
  },
});

// The next registered hook will run first (LIFO order)
```

## Load Hook

### Signature

```typescript
load(
  url: string,
  context: LoadContext,
  nextLoad: (url?: string) => LoadResult | null
): LoadResult | null | void | Promise<LoadResult | null | void>
```

### Parameters

#### `url`

The resolved module URL (e.g., `'file:///path/to/module.js'`)

#### `context`

An object containing loading context:

```typescript
interface LoadContext {
  format?: string;              // Expected format ('commonjs', 'module', 'json', etc.)
  conditions?: string[];        // Load conditions
  importAttributes?: object;    // Import attributes (for ESM)
}
```

#### `nextLoad`

Function to call the next hook in the chain, or to use default loading.

### Return Value

Return one of the following:

1. **Custom load result:**
   ```typescript
   {
     format: string;                   // Module format
     source: string | ArrayBuffer | Uint8Array;
     shortCircuit?: boolean;           // Skip remaining hooks (default: true)
   }
   ```

2. **Short-circuit with source only (legacy):**
   ```typescript
   'module.exports = { foo: "bar" };'  // Legacy format
   ```

3. **Continue processing:**
   ```typescript
   null    // Let next hook handle it
   undefined  // Same as null
   nextLoad()  // Call next hook
   ```

### Examples

```javascript
// Source code transformation
registerHooks({
  load(url, context, nextLoad) {
    if (url.includes('dev-mode')) {
      return {
        format: 'commonjs',
        source: `
          console.log('[Dev] Loading module');
          ${/* Original source would be loaded by nextLoad() */''}
        `,
        shortCircuit: true,
      };
    }
    return nextLoad();
  },
});

// Virtual module
registerHooks({
  load(url, context, nextLoad) {
    if (url.startsWith('virtual:')) {
      const moduleName = url.replace('virtual:', '');
      return {
        format: 'commonjs',
        source: `
          module.exports = {
            name: '${moduleName}',
            timestamp: Date.now()
          };
        `,
        shortCircuit: true,
      };
    }
    return nextLoad();
  },
});

// Format conversion (ESM to CommonJS)
registerHooks({
  load(url, context, nextLoad) {
    if (url.endsWith('.mjs')) {
      // Convert ESM to CommonJS
      return nextLoad().then(result => {
        return {
          format: 'commonjs',
          source: convertESMtoCJS(result.source),
          shortCircuit: true,
        };
      });
    }
    return nextLoad();
  },
});
```

## Hook Execution Order

### Registration Order (LIFO)

Hooks are executed in **Last In, First Out (LIFO)** order:

```javascript
registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log('Hook 1 (registered first)');
    return nextResolve();
  },
});

registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log('Hook 2 (registered second) - executes first');
    return nextResolve();
  },
});

// When resolving 'test':
// 1. Hook 2 executes (registered last)
// 2. Hook 2 calls nextResolve()
// 3. Hook 1 executes (registered first)
// 4. Hook 1 calls nextResolve()
// 5. Default resolution occurs
```

### Chaining with `nextResolve` / `nextLoad`

The `nextResolve` and `nextLoad` functions allow hooks to delegate to the next hook in the chain:

```javascript
registerHooks({
  resolve(specifier, context, nextResolve) {
    // Do some preprocessing
    if (specifier.startsWith('custom:')) {
      // Custom handling, don't call nextResolve
      return customResolution(specifier);
    }

    // Otherwise, let the next hook handle it
    return nextResolve();
  },
  load(url, context, nextLoad) {
    // Similar pattern for load hooks
    if (url.includes('transform-me')) {
      return transformSource(url);
    }

    return nextLoad();
  },
});
```

## Error Handling

### Hook Exceptions

If a hook function throws an exception, the error will be:

1. **Logged to stderr** with context information
2. **Wrapped** with additional context about which hook failed and what module was being loaded
3. **Propagated** to the module loading process

```javascript
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'error-prone') {
      throw new Error('Something went wrong!');
    }
    return nextResolve();
  },
});

// Error output:
// === Module Hook Error ===
// Hook Type: resolve
// Module: error-prone
// Error: Something went wrong!
// ========================
```

### With `--trace-module-hooks` Flag

Use the `--trace-module-hooks` flag to enable verbose tracing:

```bash
jsrt --trace-module-hooks my-script.js
```

This will:
- Print detailed error information including stack traces
- Log hook execution flow
- Show which hooks are called and in what order

## Use Cases

### 1. Development/Production Customization

```javascript
const isDevelopment = process.env.NODE_ENV === 'development';

registerHooks({
  resolve(specifier, context, nextResolve) {
    if (isDevelopment) {
      // Use development versions
      if (specifier === 'lib') {
        return { url: './src/lib-dev.js', format: 'commonjs', shortCircuit: true };
      }
    } else {
      // Use production versions
      if (specifier === 'lib') {
        return { url: './dist/lib.min.js', format: 'commonjs', shortCircuit: true };
      }
    }
    return nextResolve();
  },
});
```

### 2. TypeScript Compilation

```javascript
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier.endsWith('.ts')) {
      return {
        url: specifier.replace('.ts', '.js'),
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve();
  },
  load(url, context, nextLoad) {
    if (url.includes('src/') && url.endsWith('.js')) {
      // Compile TypeScript on the fly
      return nextLoad().then(result => {
        return {
          format: 'commonjs',
          source: typescriptCompiler.compile(result.source),
          shortCircuit: true,
        };
      });
    }
    return nextLoad();
  },
});
```

### 3. Mocking for Testing

```javascript
registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'database') {
      return {
        url: 'virtual:mock-database',
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve();
  },
  load(url, context, nextLoad) {
    if (url === 'virtual:mock-database') {
      return {
        format: 'commonjs',
        source: `
          const mockData = require('./test/fixtures/mock-data.json');
          module.exports = {
            async query(sql) { return mockData; },
            async find(id) { return mockData.find(x => x.id === id); }
          };
        `,
        shortCircuit: true,
      };
    }
    return nextLoad();
  },
});
```

## Best Practices

1. **Always call `nextResolve()` or `nextLoad()` when not handling the request**
   - This allows other hooks and default behavior to work correctly

2. **Use `shortCircuit: true` when providing complete results**
   - This improves performance by skipping remaining hooks

3. **Handle errors gracefully**
   - Wrap hook code in try-catch blocks
   - Provide meaningful error messages

4. **Be mindful of hook ordering (LIFO)**
   - Document the order dependencies
   - Consider using named hooks for clarity

5. **Avoid infinite loops**
   - Don't call `nextResolve()`/`nextLoad()` with the same arguments you're processing
   - Set appropriate `shortCircuit` flags

6. **Cache expensive operations**
   - Resolve/load hooks can be called frequently
   - Cache custom resolution results when possible

## Limitations

- **Synchronous hooks only** - Async hooks are planned for future versions
- **No unregistration API** - Once registered, hooks remain active for the lifetime of the runtime
- **Performance impact** - Each hook adds overhead to module loading

## Debugging

### Enable Trace Mode

```bash
jsrt --trace-module-hooks my-script.js
```

This will output:
- Which hooks are called
- Hook execution order
- Detailed error information with stack traces
- Module resolution/loading flow

### Log to stderr

Hook errors are automatically logged to stderr with context:

```
=== Module Hook Error ===
Hook Type: resolve
Module: my-module
Error: Custom resolution failed
Stack Trace:
  at resolveHook (my-script.js:45:12)
  at ...
========================
```

## Examples

See the `examples/module/` directory for working examples:

- `hooks-resolve.js` - Resolve hook demonstrations
- `hooks-load.js` - Load hook demonstrations
- `hooks-transform.js` - Complete transformation pipeline

Run examples:

```bash
jsrt examples/module/hooks-resolve.js
jsrt --trace-module-hooks examples/module/hooks-load.js
```

## TypeScript Support

Type definitions for hooks are available in the TypeScript type definitions.

```typescript
import { registerHooks } from 'node:module';

registerHooks({
  resolve(specifier, context, nextResolve) {
    // Type-checked parameters and return values
  },
  load(url, context, nextLoad) {
    // Type-checked parameters and return values
  },
});
```
