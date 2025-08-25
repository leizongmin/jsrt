// Test dynamic import
import('./test_module.mjs')
  .then(mod => {
    console.log('Dynamic Import Test:');
    console.log(mod.hello('Dynamic World'));
    console.log('Dynamic Value:', mod.value);
  })
  .catch(err => {
    console.error('Dynamic import error:', err);
  });