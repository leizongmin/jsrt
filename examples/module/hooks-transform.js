#!/usr/bin/env jsrt

/**
 * Example: Module Hook Transformation Pipeline
 *
 * This example demonstrates a practical use case combining resolve and load hooks
 * to create a transformation pipeline for modules.
 *
 * Use cases:
 * - Development: Add debug logging, source maps
 * - Testing: Mock external dependencies
 * - Security: Strip sensitive code in production
 * - Custom languages: Compile TypeScript, CoffeeScript, etc.
 */

const { registerHooks } = require('node:module');

console.log('=== Module Transformation Pipeline Example ===\n');

// Use case 1: Development mode transformation
console.log('1. Development Mode Transformations:\n');

let isDevelopment = true;

registerHooks({
  resolve(specifier, context, nextResolve) {
    console.log(`   [Resolve] ${specifier}`);

    // Example: Mock certain modules during development
    if (specifier === 'database' || specifier === 'db') {
      console.log('   [Resolve] -> Mocking database module');
      return {
        url: 'virtual:mock-database',
        format: 'commonjs',
        shortCircuit: true,
      };
    }

    // Example: Redirect config files
    if (specifier.includes('config')) {
      if (isDevelopment) {
        console.log('   [Resolve] -> Using dev config');
        return {
          url: './config/development.json',
          format: 'json',
          shortCircuit: true,
        };
      } else {
        console.log('   [Resolve] -> Using production config');
        return {
          url: './config/production.json',
          format: 'json',
          shortCircuit: true,
        };
      }
    }

    return nextResolve();
  },

  load(url, context, nextLoad) {
    console.log(`   [Load] ${url}`);

    // Example: Add debug wrappers for development
    if (isDevelopment && url.startsWith('./src/')) {
      console.log('   [Load] -> Adding debug wrapper');

      return nextLoad().then((result) => {
        return {
          format: 'commonjs',
          source: `
            console.log('[Debug] Entering module:', '${url}');
            try {
              ${result.source}
              console.log('[Debug] Exiting module:', '${url}');
            } catch (error) {
              console.error('[Debug] Error in module:', '${url}', error);
              throw error;
            }
          `,
          shortCircuit: true,
        };
      });
    }

    // Example: Load mocked database
    if (url === 'virtual:mock-database') {
      console.log('   [Load] -> Creating mock database');
      return {
        format: 'commonjs',
        source: `
          const mockData = {
            users: [
              { id: 1, name: 'Alice', email: 'alice@example.com' },
              { id: 2, name: 'Bob', email: 'bob@example.com' },
            ],
          };

          module.exports = {
            async query(sql) {
              console.log('[Mock DB] Executing:', sql);
              return mockData.users;
            },
            async findById(id) {
              console.log('[Mock DB] Finding user:', id);
              return mockData.users.find(u => u.id === id);
            }
          };
        `,
        shortCircuit: true,
      };
    }

    // Example: Inject timestamp into JSON files
    if (url.endsWith('.json')) {
      console.log('   [Load] -> Injecting metadata into JSON');

      return nextLoad().then((result) => {
        try {
          const parsed = JSON.parse(result.source);
          parsed._meta = {
            loadedAt: new Date().toISOString(),
            source: url,
            environment: isDevelopment ? 'development' : 'production',
          };
          return {
            format: 'json',
            source: JSON.stringify(parsed, null, 2),
            shortCircuit: true,
          };
        } catch (e) {
          console.log('   [Load] -> Invalid JSON, passing through');
          return result;
        }
      });
    }

    return nextLoad();
  },
});

// Test the transformation pipeline
console.log('   Testing development mode:\n');
isDevelopment = true;

const fs = require('fs');

// Create test config file
fs.writeFileSync('./tmp-config.json', '{"name":"test","value":123}');

try {
  console.log('   Requiring config...');
  const config = require('./tmp-config.json');
  console.log(`   Config loaded: ${JSON.stringify(config)}\n`);
} catch (e) {
  console.log(`   Error: ${e.message}\n`);
}

try {
  console.log('   Requiring mocked database...');
  const db = require('database');
  console.log(`   Mock DB loaded: ${typeof db}\n`);
} catch (e) {
  console.log(`   Error: ${e.message}\n`);
}

console.log('2. Production Mode Transformations:\n');

// Use case 2: Production optimizations
console.log('   Switching to production mode:\n');
isDevelopment = false;

registerHooks({
  resolve(specifier, context, nextResolve) {
    // Production: Strip debug code
    if (specifier.includes('debug-utils')) {
      console.log('   [Resolve] -> Using production version of debug-utils');
      return {
        url: './vendor/debug-utils.production.js',
        format: 'commonjs',
        shortCircuit: true,
      };
    }

    return nextResolve();
  },

  load(url, context, nextLoad) {
    if (url.includes('.production.js')) {
      console.log(`   [Load] -> Loading production version`);

      return nextLoad().then((result) => {
        // Strip console.log statements
        const stripped = result.source.replace(/console\.log\([^)]*\);?/g, '');
        return {
          format: 'commonjs',
          source: stripped,
          shortCircuit: true,
        };
      });
    }

    return nextLoad();
  },
});

console.log('3. Language Compilation Pipeline:\n');

// Use case 3: Compile TypeScript to JavaScript
registerHooks({
  resolve(specifier, context, nextResolve) {
    // Example: Compile TypeScript on-the-fly
    if (specifier.endsWith('.ts')) {
      console.log(`   [Resolve] -> TypeScript file detected`);
      return {
        url: specifier.replace('.ts', '.js'),
        format: 'commonjs',
        shortCircuit: true,
      };
    }

    return nextResolve();
  },

  load(url, context, nextLoad) {
    if (url.endsWith('.js') && url.includes('src-ts/')) {
      console.log(`   [Load] -> Compiling TypeScript to JavaScript`);

      // In a real scenario, you'd use the TypeScript compiler
      return nextLoad().then((result) => {
        return {
          format: 'commonjs',
          source: `
            // [TypeScript Compilation]
            // Original: ${url}
            // In real implementation, this would compile .ts to .js
            ${result.source}
          `,
          shortCircuit: true,
        };
      });
    }

    return nextLoad();
  },
});

console.log('\n=== Transformation Pipeline Examples Complete ===');
console.log('\nKey Patterns:');
console.log('1. Resolve hooks: Redirect, mock, or transform specifiers');
console.log(
  '2. Load hooks: Transform source code, add metadata, compile languages'
);
console.log('3. Combine both: Full transformation pipeline');
console.log('4. Environment-aware: Different behavior for dev/prod');
console.log('5. Virtual modules: Create modules from nothing');

console.log('\nReal-world applications:');
console.log('- TypeScript/CoffeeScript compilation');
console.log('- Framework-specific transformations');
console.log('- A/B testing (different implementations)');
console.log('- Feature flags and toggles');
console.log('- Security code scanning and filtering');
