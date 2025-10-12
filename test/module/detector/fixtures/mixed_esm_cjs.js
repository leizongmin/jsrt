// Mixed file with both ESM and CommonJS patterns
import { foo } from './foo.js';

// Also has legacy require for compatibility
const legacy = require('./legacy.js');

export function bar() {
  return foo() + legacy.value;
}
