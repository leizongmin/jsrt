import {
  join,
  normalize,
  relative,
  dirname,
  basename,
  extname,
  isAbsolute,
} from 'node:path';

// Test basic functionality
const result1 = join('a', 'b');
const result2 = normalize('/a/b/../c');
const result3 = relative('/a/b', '/a/c');
const result4 = dirname('/a/b/c');
const result5 = basename('file.txt');
const result6 = extname('file.txt');
const result7 = isAbsolute('/foo');

// Verify results exist (basic smoke test)
if (
  !result1 ||
  !result2 ||
  !result3 ||
  !result4 ||
  !result5 ||
  !result6 ||
  typeof result7 !== 'boolean'
) {
  console.log('❌ ES module imports failed');
}
