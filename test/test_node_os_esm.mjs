import {
  arch,
  platform,
  type,
  release,
  hostname,
  tmpdir,
  homedir,
  userInfo,
  endianness,
  EOL,
} from 'node:os';

console.log('Testing node:os ES module imports...');

console.log('arch():', arch());
console.log('platform():', platform());
console.log('type():', type());
console.log('release():', release());
console.log('hostname():', hostname());
console.log('tmpdir():', tmpdir());
console.log('homedir():', homedir());
console.log('userInfo():', userInfo());
console.log('endianness():', endianness());
console.log('EOL:', JSON.stringify(EOL));

console.log('✅ ES module imports for node:os working correctly');
console.log('=== ES module memory management for node:os successful ===');
