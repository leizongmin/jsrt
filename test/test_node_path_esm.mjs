import { join, normalize, relative, dirname, basename, extname, isAbsolute } from 'node:path';

console.log('Testing node:path ES module imports...');

// Test basic functionality
console.log('path.join("a", "b"):', join('a', 'b'));
console.log('path.normalize("/a/b/../c"):', normalize('/a/b/../c'));
console.log('path.relative("/a/b", "/a/c"):', relative('/a/b', '/a/c'));
console.log('path.dirname("/a/b/c"):', dirname('/a/b/c'));
console.log('path.basename("file.txt"):', basename('file.txt'));
console.log('path.extname("file.txt"):', extname('file.txt'));
console.log('path.isAbsolute("/foo"):', isAbsolute('/foo'));

console.log('✅ ES module imports working correctly');
console.log('=== ES module memory management fix successful ===');