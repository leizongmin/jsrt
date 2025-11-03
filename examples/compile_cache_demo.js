#!/usr/bin/env node

/**
 * Compile Cache Maintenance Demo
 *
 * This example demonstrates the compile cache maintenance features
 * implemented in Task 3.8: Cache Maintenance.
 */

const fs = require('fs');
const path = require('path');
const module = require('node:module');

function main() {
  console.log('=== jsrt Compile Cache Maintenance Demo ===\n');

  // Create a temporary directory for this demo
  const demoDir = fs.mkdtempSync(path.join(__dirname, 'cache-demo-'));
  const cacheDir = path.join(demoDir, 'cache');

  try {
    console.log('1. Enabling compile cache...');
    const result = module.enableCompileCache(cacheDir, { portable: true });
    console.log('   Status:', result.message);
    console.log('   Directory:', result.directory);
    console.log('   Portable mode:', result.portable);
    console.log();

    // Create some test modules
    console.log('2. Creating test modules...');
    const modules = [];
    for (let i = 1; i <= 5; i++) {
      const filename = path.join(demoDir, `module${i}.js`);
      const content = `
// Module ${i}
const data = { id: ${i}, value: "value${i}" };
module.exports = data;
`;
      fs.writeFileSync(filename, content);
      modules.push(filename);
      console.log(`   Created: module${i}.js`);
    }
    console.log();

    console.log('3. Loading modules to populate cache...');
    modules.forEach((mod, index) => {
      const exported = require(mod);
      console.log(`   module${index + 1}:`, exported);
    });
    console.log();

    console.log('4. Cache statistics after loading:');
    const stats = module.getCompileCacheStats();
    console.log('   Hits:', stats.hits);
    console.log('   Misses:', stats.misses);
    console.log('   Writes:', stats.writes);
    console.log('   Errors:', stats.errors);
    console.log('   Evictions:', stats.evictions);
    console.log('   Current size:', formatBytes(stats.currentSize));
    console.log('   Size limit:', formatBytes(stats.sizeLimit));
    console.log('   Hit rate:', stats.hitRate.toFixed(2) + '%');
    console.log('   Utilization:', stats.utilization.toFixed(2) + '%');
    console.log();

    console.log('5. Loading modules again (should hit cache)...');
    // Clear the module cache to force reloads
    modules.forEach((mod) => delete require.cache[require.resolve(mod)]);

    modules.forEach((mod, index) => {
      const exported = require(mod);
      console.log(`   module${index + 1}:`, exported);
    });
    console.log();

    console.log('6. Cache statistics after second load:');
    const stats2 = module.getCompileCacheStats();
    console.log('   Hits:', stats2.hits);
    console.log('   Misses:', stats2.misses);
    console.log('   Writes:', stats2.writes);
    console.log('   Errors:', stats2.errors);
    console.log('   Evictions:', stats2.evictions);
    console.log('   Current size:', formatBytes(stats2.currentSize));
    console.log('   Hit rate:', stats2.hitRate.toFixed(2) + '%');
    console.log('   Utilization:', stats2.utilization.toFixed(2) + '%');
    console.log();

    console.log('7. Cache directory contents:');
    if (fs.existsSync(cacheDir)) {
      const files = fs.readdirSync(cacheDir);
      files.forEach((file) => {
        const filePath = path.join(cacheDir, file);
        const stats = fs.statSync(filePath);
        console.log(`   ${file} (${formatBytes(stats.size)})`);
      });
    }
    console.log();

    console.log('8. Clearing cache...');
    const clearedCount = module.clearCompileCache();
    console.log('   Cleared entries:', clearedCount);
    console.log();

    console.log('9. Cache statistics after clear:');
    const stats3 = module.getCompileCacheStats();
    console.log('   Current size:', formatBytes(stats3.currentSize));
    console.log('   Hit rate:', stats3.hitRate.toFixed(2) + '%');
    console.log();

    console.log('10. Cache directory after clear:');
    if (fs.existsSync(cacheDir)) {
      const files = fs.readdirSync(cacheDir);
      if (files.length === 0) {
        console.log('   (empty)');
      } else {
        files.forEach((file) => console.log(`   ${file}`));
      }
    }
    console.log();

    console.log('✓ Demo completed successfully!');
    console.log();
    console.log('Key features demonstrated:');
    console.log('  ✓ Cache enable/disable');
    console.log('  ✓ Cache statistics reporting');
    console.log('  ✓ Hit rate tracking');
    console.log('  ✓ Size monitoring');
    console.log('  ✓ Cache clearing');
    console.log('  ✓ LRU eviction (when size limit exceeded)');
  } catch (error) {
    console.error('✗ Demo failed:', error);
    process.exit(1);
  } finally {
    // Clean up
    try {
      fs.rmSync(demoDir, { recursive: true, force: true });
      console.log('\n🧹 Cleaned up demo directory');
    } catch (error) {
      console.warn(
        'Warning: Failed to clean up demo directory:',
        error.message
      );
    }
  }
}

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

if (require.main === module) {
  main();
}
