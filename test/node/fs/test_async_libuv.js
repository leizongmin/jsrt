// Test fs async operations with libuv (Phase 2 POC)

const fs = require('node:fs');
const path = require('node:path');

// Test directory
const testDir = 'target/tmp/fs_async_test';
const testFile = path.join(testDir, 'test.txt');
const testFile2 = path.join(testDir, 'test2.txt');

let passCount = 0;
let failCount = 0;

function pass(msg) {
  console.log(`✓ PASS: ${msg}`);
  passCount++;
}

function fail(msg, error) {
  console.log(`✗ FAIL: ${msg}`);
  if (error) console.log(`  Error: ${error.message || error}`);
  failCount++;
  process.exit(1); // Exit immediately on failure
}

// Cleanup and setup
try {
  if (fs.existsSync(testDir)) {
    fs.rmSync(testDir, { recursive: true });
  }
  fs.mkdirSync(testDir, { recursive: true });
  pass('Setup test directory');
} catch (e) {
  fail('Setup test directory', e);
}

// Test 1: fs.writeFile (async with libuv)
console.log('\n--- Test 1: fs.writeFile (async) ---');
fs.writeFile(testFile, 'Hello, libuv async!', (err) => {
  if (err) {
    fail('writeFile async', err);
  } else {
    pass('writeFile async - callback executed');

    // Test 2: fs.readFile (async with libuv)
    console.log('\n--- Test 2: fs.readFile (async) ---');
    fs.readFile(testFile, (err, data) => {
      if (err) {
        fail('readFile async', err);
      } else {
        const content = data.toString();
        if (content === 'Hello, libuv async!') {
          pass('readFile async - content matches');
        } else {
          fail(`readFile async - content mismatch: "${content}"`);
        }

        // Test 3: fs.rename (async)
        console.log('\n--- Test 3: fs.rename (async) ---');
        fs.rename(testFile, testFile2, (err) => {
          if (err) {
            fail('rename async', err);
          } else {
            pass('rename async - callback executed');

            // Verify rename worked
            if (fs.existsSync(testFile2) && !fs.existsSync(testFile)) {
              pass('rename async - file moved correctly');
            } else {
              fail('rename async - file not moved');
            }

            // Test 4: fs.access (async)
            console.log('\n--- Test 4: fs.access (async) ---');
            fs.access(testFile2, fs.constants.F_OK, (err) => {
              if (err) {
                fail('access async - file should exist', err);
              } else {
                pass('access async - file exists');

                // Test 5: fs.access on non-existent file
                console.log('\n--- Test 5: fs.access (non-existent) ---');
                fs.access('nonexistent.txt', fs.constants.F_OK, (err) => {
                  if (err) {
                    pass('access async - correctly reports ENOENT');
                  } else {
                    fail(
                      'access async - should have failed for non-existent file'
                    );
                  }

                  // Test 6: fs.unlink (async)
                  console.log('\n--- Test 6: fs.unlink (async) ---');
                  fs.unlink(testFile2, (err) => {
                    if (err) {
                      fail('unlink async', err);
                    } else {
                      pass('unlink async - callback executed');

                      if (!fs.existsSync(testFile2)) {
                        pass('unlink async - file deleted');
                      } else {
                        fail('unlink async - file still exists');
                      }

                      // Test 7: fs.mkdir (async)
                      console.log('\n--- Test 7: fs.mkdir (async) ---');
                      const newDir = path.join(testDir, 'subdir');
                      fs.mkdir(newDir, (err) => {
                        if (err) {
                          fail('mkdir async', err);
                        } else {
                          pass('mkdir async - callback executed');

                          if (fs.existsSync(newDir)) {
                            pass('mkdir async - directory created');
                          } else {
                            fail('mkdir async - directory not created');
                          }

                          // Test 8: fs.rmdir (async)
                          console.log('\n--- Test 8: fs.rmdir (async) ---');
                          fs.rmdir(newDir, (err) => {
                            if (err) {
                              fail('rmdir async', err);
                            } else {
                              pass('rmdir async - callback executed');

                              if (!fs.existsSync(newDir)) {
                                pass('rmdir async - directory removed');
                              } else {
                                fail('rmdir async - directory still exists');
                              }

                              // Summary
                              console.log('\n=== Test Summary ===');
                              console.log(`PASS: ${passCount}`);
                              console.log(`FAIL: ${failCount}`);
                              if (failCount > 0) {
                                process.exit(1);
                              } else {
                                console.log(
                                  '\n✓ All async libuv tests passed!'
                                );
                                process.exit(0);
                              }
                            }
                          });
                        }
                      });
                    }
                  });
                });
              }
            });
          }
        });
      }
    });
  }
});

// Keep event loop alive
setTimeout(() => {
  console.log(
    '\n⚠ WARNING: Test timeout - some callbacks may not have executed'
  );
  console.log(`Final: PASS: ${passCount}, FAIL: ${failCount}`);
  process.exit(1);
}, 2000);
