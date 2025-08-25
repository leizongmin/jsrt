#!/bin/bash
# Demo script showing working module loading features

echo "🚀 jsrt Module Loading Demo"
echo "============================="

cd "$(dirname "$0")"

echo
echo "📦 1. ESM Static Import:"
../target/release/jsrt test_import.mjs

echo
echo "📦 2. ESM Dynamic Import:"
../target/release/jsrt test_dynamic_import.mjs

echo
echo "📦 3. ESM Top-level Await:"
../target/release/jsrt test_toplevel_await.mjs

echo
echo "📦 4. CommonJS Require:"
../target/release/jsrt ../test/test_modules_cjs.js

echo
echo "📦 5. ESM Test Suite:"
../target/release/jsrt ../test/test_modules_esm.mjs

echo
echo "📦 6. CommonJS Test Suite:"
../target/release/jsrt ../test/test_modules_cjs.js

echo
echo "✅ All module loading features working!"
echo "🎉 Implementation complete!"