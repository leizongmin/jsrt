// Basic sanity check for WebAssembly.instantiateStreaming fallback logic

const fakeResponse = {
  arrayBuffer() {
    fakeResponse.called = true;
    return Promise.reject(new Error('no bytes available'));
  },
  called: false,
};

async function run() {
  console.log(
    'Test: WebAssembly.instantiateStreaming returns a Promise and invokes arrayBuffer'
  );

  const promise = WebAssembly.instantiateStreaming(fakeResponse);
  if (!promise || typeof promise.then !== 'function') {
    throw new Error('instantiateStreaming should return a Promise');
  }

  let rejected = false;
  try {
    await promise;
  } catch (err) {
    rejected = true;
  }

  if (!fakeResponse.called) {
    throw new Error(
      'instantiateStreaming did not call arrayBuffer on the source'
    );
  }
  if (!rejected) {
    throw new Error(
      'instantiateStreaming should propagate rejection from arrayBuffer'
    );
  }

  console.log(
    '  PASS: instantiateStreaming triggered arrayBuffer and returned a Promise'
  );
}

run()
  .then(() => console.log('Done.'))
  .catch((err) => {
    console.error('FAIL:', err && err.stack ? err.stack : err);
    throw err;
  });
