// Example: Using process.cwd() and process.chdir()
// This demonstrates how to get and change the current working directory

console.log('=== Process Working Directory Example ===');

// Get current working directory
const originalDir = process.cwd();
console.log('Original working directory:', originalDir);

// Try to change to a subdirectory
try {
  console.log("\nChanging to 'src' directory...");
  process.chdir('src');
  console.log('New working directory:', process.cwd());

  // List some files in the current directory (if available)
  console.log('\nWe are now in the src directory!');

  // Change back to parent directory
  console.log('\nChanging back to parent directory...');
  process.chdir('..');
  console.log('Back to:', process.cwd());
} catch (error) {
  console.error('Error changing directory:', error.message);
}

// Demonstrate error handling
console.log('\n=== Error Handling Examples ===');

// Try to change to a non-existent directory
try {
  process.chdir('/this/path/does/not/exist');
} catch (error) {
  console.log(
    '✓ Caught expected error for non-existent directory:',
    error.message
  );
}

// Try to call chdir without arguments
try {
  process.chdir();
} catch (error) {
  console.log('✓ Caught expected error for missing argument:', error.message);
}

// Restore original directory
try {
  process.chdir(originalDir);
  console.log('\n✓ Restored to original directory:', process.cwd());
} catch (error) {
  console.error('Failed to restore original directory:', error.message);
}

console.log('\n=== Example completed ===');
