console.log('=== ReadableStream Debug ===');
console.log('ReadableStream:', typeof ReadableStream);
if (typeof ReadableStream !== 'undefined') {
  const rs = new ReadableStream();
  const reader = rs.getReader();
  console.log('reader:', typeof reader);
  console.log('reader.closed:', typeof reader.closed);
  console.log('reader.closed then:', typeof reader.closed?.then);
}

console.log('\n=== WritableStream Debug ===');  
console.log('WritableStream:', typeof WritableStream);
if (typeof WritableStream !== 'undefined') {
  console.log('WritableStream constructor:', typeof WritableStream);
  try {
    const writer = new WritableStream().getWriter();
    console.log('writer type:', typeof writer);
  } catch (e) {
    console.log('Error creating writer:', e.message);
  }
}

console.log('\n=== AbortSignal Debug ===');
console.log('AbortSignal:', typeof AbortSignal);
console.log('AbortSignal.any:', typeof AbortSignal.any);
console.log('AbortSignal.timeout:', typeof AbortSignal.timeout);