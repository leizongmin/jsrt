// Test WebAssembly exported Table (instance.exports.table)
// Exported tables can be accessed but have limited operations due to WAMR

console.log('Test 1: Exported Table - basic access');
{
  // (module (table (export "table") 10 funcref))
  const wasmBytes = new Uint8Array([
    0x00,
    0x61,
    0x73,
    0x6d, // WASM magic
    0x01,
    0x00,
    0x00,
    0x00, // WASM version
    // Table section
    0x04, // section code 4 (table)
    0x04, // section size
    0x01, // 1 table
    0x70, // element type: funcref
    0x00, // flags (no max)
    0x0a, // initial: 10
    // Export section
    0x07, // section code 7 (export)
    0x09, // section size
    0x01, // 1 export
    0x05, // name length
    0x74,
    0x61,
    0x62,
    0x6c,
    0x65, // "table"
    0x01, // export kind: table
    0x00, // table index 0
  ]);

  const module = new WebAssembly.Module(wasmBytes);
  const instance = new WebAssembly.Instance(module);

  console.log('  Instance created');
  console.log('  Exports:', Object.keys(instance.exports));

  if (instance.exports.table) {
    console.log('  ✓ Table found in exports');
    const table = instance.exports.table;
    console.log('  ✓ Type:', typeof table);

    if (!(table instanceof WebAssembly.Table)) {
      throw new Error('Not a Table object');
    }
    console.log('  ✓ Is Table instance');

    // Test length property
    if (typeof table.length === 'number') {
      console.log('  ✓ Has length property');
      console.log('  ✓ Table length:', table.length);

      if (table.length !== 10) {
        throw new Error(`Expected length 10, got ${table.length}`);
      }
      console.log('  ✓ Length is correct (10)');
    } else {
      throw new Error('No length property');
    }

    // Note: get/set/grow operations are not supported for exported tables
    // This is a WAMR limitation - the C API methods don't work on exported table instances

    console.log('  PASS: Exported Table basic access works');
  } else {
    throw new Error('Table not in exports');
  }
}

console.log('\n========================================');
console.log('Exported Table tests completed!');
console.log('Key finding: Instance-exported tables are accessible');
console.log('Note: get/set/grow operations limited by WAMR API');
console.log('========================================');
