// Debug async issue step by step
console.log('Step 1: Basic async function');

async function step1() {
  console.log('Inside step1');
  return 'step1 complete';
}

console.log('Step 2: Call async function with await');

async function step2() {
  console.log('Inside step2');
  const result = await step1();
  console.log('Step1 result:', result);
  return 'step2 complete';
}

console.log('Step 3: Main function');

async function main() {
  console.log('Inside main');
  const result = await step2();
  console.log('Step2 result:', result);
  console.log('Main completed');
}

console.log('Step 4: Execute main');
main();
