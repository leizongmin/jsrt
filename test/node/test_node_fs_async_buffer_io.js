// Test fs.read() and fs.write() async buffer I/O operations
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { Buffer } = require('node:buffer');

function assert(condition, message) {
  if (!condition) {
    throw new Error(`Assertion failed: ${message}`);
  }
}

// Get tmpdir from os module
const tmpdir = os.tmpdir();

// Test 1: fs.write() - write data to a file using buffer I/O
console.log('Test 1: fs.write() async buffer I/O');
const fd1 = fs.openSync(path.join(tmpdir, 'test_write.txt'), 'w');
const writeBuffer = Buffer.from('Hello from async write!');

fs.write(
  fd1,
  writeBuffer,
  0,
  writeBuffer.length,
  0,
  (err, bytesWritten, buffer) => {
    if (err) {
      console.error('fs.write() error:', err);
      process.exit(1);
    }

    console.log(`  Wrote ${bytesWritten} bytes`);
    assert(
      bytesWritten === writeBuffer.length,
      'bytes written should match buffer length'
    );

    fs.closeSync(fd1);

    // Test 2: fs.read() - read data from file using buffer I/O
    console.log('Test 2: fs.read() async buffer I/O');
    const fd2 = fs.openSync(path.join(tmpdir, 'test_write.txt'), 'r');
    const readBuffer = Buffer.alloc(100);

    fs.read(fd2, readBuffer, 0, 100, 0, (err, bytesRead, buffer) => {
      if (err) {
        console.error('fs.read() error:', err);
        process.exit(1);
      }

      console.log(`  Read ${bytesRead} bytes`);
      const content = buffer.toString('utf8', 0, bytesRead);
      console.log(`  Content: "${content}"`);

      assert(content === 'Hello from async write!', 'content should match');
      assert(
        bytesRead === writeBuffer.length,
        'bytes read should match bytes written'
      );

      fs.closeSync(fd2);

      // Test 3: fs.write() with position
      console.log('Test 3: fs.write() with specific position');
      const fd3 = fs.openSync(path.join(tmpdir, 'test_position.txt'), 'w');
      const buf1 = Buffer.from('Hello');
      const buf2 = Buffer.from('World');

      fs.write(fd3, buf1, 0, buf1.length, 0, (err1, bytes1) => {
        if (err1) {
          console.error('First write error:', err1);
          process.exit(1);
        }

        // Write second buffer at position 6 (after "Hello ")
        fs.write(fd3, buf2, 0, buf2.length, 6, (err2, bytes2) => {
          if (err2) {
            console.error('Second write error:', err2);
            process.exit(1);
          }

          fs.closeSync(fd3);

          // Verify the result
          const fd4 = fs.openSync(path.join(tmpdir, 'test_position.txt'), 'r');
          const verifyBuf = Buffer.alloc(20);

          fs.read(fd4, verifyBuf, 0, 20, 0, (err3, bytesRead3) => {
            if (err3) {
              console.error('Verify read error:', err3);
              process.exit(1);
            }

            const result = verifyBuf.toString('utf8', 0, bytesRead3);
            console.log(`  Result: "${result}"`);

            // Should be "Hello\x00World" because position 6 leaves a gap
            assert(result.startsWith('Hello'), 'should start with Hello');
            assert(result.includes('World'), 'should include World');

            fs.closeSync(fd4);

            // Test 4: fs.read() with null position (current position)
            console.log('Test 4: fs.read() with null position');
            const fd5 = fs.openSync(path.join(tmpdir, 'test_write.txt'), 'r');
            const seqBuf1 = Buffer.alloc(5);
            const seqBuf2 = Buffer.alloc(10);

            fs.read(fd5, seqBuf1, 0, 5, null, (err4, bytes4) => {
              if (err4) {
                console.error('Sequential read 1 error:', err4);
                process.exit(1);
              }

              const part1 = seqBuf1.toString('utf8');
              console.log(`  First read: "${part1}"`);
              assert(part1 === 'Hello', 'first read should be Hello');

              // Second read should continue from current position
              fs.read(fd5, seqBuf2, 0, 10, null, (err5, bytes5) => {
                if (err5) {
                  console.error('Sequential read 2 error:', err5);
                  process.exit(1);
                }

                const part2 = seqBuf2.toString('utf8', 0, bytes5);
                console.log(`  Second read: "${part2}"`);
                assert(
                  part2 === ' from asy',
                  'second read should continue from position'
                );

                fs.closeSync(fd5);

                // Test 5: fs.readv() - vectored read
                console.log('Test 5: fs.readv() vectored read');
                const fd6 = fs.openSync(path.join(tmpdir, 'test_readv.txt'), 'w');
                fs.writeFileSync(path.join(tmpdir, 'test_readv.txt'), 'AAAABBBBCCCCDDDD');
                fs.closeSync(fd6);

                const fd7 = fs.openSync(path.join(tmpdir, 'test_readv.txt'), 'r');
                const bufs = [
                  Buffer.alloc(4),
                  Buffer.alloc(4),
                  Buffer.alloc(4),
                  Buffer.alloc(4),
                ];

                fs.readv(fd7, bufs, 0, (errReadv, bytesReadv, bufsReadv) => {
                  if (errReadv) {
                    console.error('fs.readv() error:', errReadv);
                    process.exit(1);
                  }

                  console.log(
                    `  Read ${bytesReadv} bytes into ${bufsReadv.length} buffers`
                  );
                  assert(bytesReadv === 16, 'should read 16 bytes');
                  assert(
                    bufsReadv[0].toString() === 'AAAA',
                    'buffer 0 should be AAAA'
                  );
                  assert(
                    bufsReadv[1].toString() === 'BBBB',
                    'buffer 1 should be BBBB'
                  );
                  assert(
                    bufsReadv[2].toString() === 'CCCC',
                    'buffer 2 should be CCCC'
                  );
                  assert(
                    bufsReadv[3].toString() === 'DDDD',
                    'buffer 3 should be DDDD'
                  );

                  fs.closeSync(fd7);

                  // Test 6: fs.writev() - vectored write
                  console.log('Test 6: fs.writev() vectored write');
                  const fd8 = fs.openSync(path.join(tmpdir, 'test_writev.txt'), 'w');
                  const writeBufs = [
                    Buffer.from('Part1'),
                    Buffer.from('Part2'),
                    Buffer.from('Part3'),
                  ];

                  fs.writev(
                    fd8,
                    writeBufs,
                    0,
                    (errWritev, bytesWritev, bufsWritev) => {
                      if (errWritev) {
                        console.error('fs.writev() error:', errWritev);
                        process.exit(1);
                      }

                      console.log(
                        `  Wrote ${bytesWritev} bytes from ${bufsWritev.length} buffers`
                      );
                      assert(bytesWritev === 15, 'should write 15 bytes');

                      fs.closeSync(fd8);

                      const written = fs.readFileSync(
                        path.join(tmpdir, 'test_writev.txt'),
                        'utf8'
                      );
                      assert(
                        written === 'Part1Part2Part3',
                        'writev should write all parts'
                      );

                      // Test 7: fs.rm() - remove file
                      console.log('Test 7: fs.rm() remove file');
                      fs.writeFileSync(path.join(tmpdir, 'test_rm.txt'), 'delete me');

                      fs.rm(path.join(tmpdir, 'test_rm.txt'), (errRm) => {
                        if (errRm) {
                          console.error('fs.rm() error:', errRm);
                          process.exit(1);
                        }

                        console.log('  File removed successfully');
                        assert(
                          !fs.existsSync(path.join(tmpdir, 'test_rm.txt')),
                          'file should be removed'
                        );

                        // Test 8: fs.cp() - copy file
                        console.log('Test 8: fs.cp() copy file');
                        fs.writeFileSync(path.join(tmpdir, 'test_cp_src.txt'), 'copy this');

                        fs.cp(
                          path.join(tmpdir, 'test_cp_src.txt'),
                          path.join(tmpdir, 'test_cp_dest.txt'),
                          (errCp) => {
                            if (errCp) {
                              console.error('fs.cp() error:', errCp);
                              process.exit(1);
                            }

                            console.log('  File copied successfully');
                            assert(
                              fs.existsSync(path.join(tmpdir, 'test_cp_dest.txt')),
                              'destination should exist'
                            );

                            const copiedContent = fs.readFileSync(
                              path.join(tmpdir, 'test_cp_dest.txt'),
                              'utf8'
                            );
                            assert(
                              copiedContent === 'copy this',
                              'copied content should match'
                            );

                            // Cleanup
                            fs.unlinkSync(path.join(tmpdir, 'test_cp_src.txt'));
                            fs.unlinkSync(path.join(tmpdir, 'test_cp_dest.txt'));
                            fs.unlinkSync(path.join(tmpdir, 'test_readv.txt'));
                            fs.unlinkSync(path.join(tmpdir, 'test_writev.txt'));

                            console.log('✓ All async buffer I/O tests passed!');
                          }
                        );
                      });
                    }
                  );
                });
              });
            });
          });
        });
      });
    });
  }
);
