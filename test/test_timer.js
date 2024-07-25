const a = {v: 0};
const t1 = setTimeout((a, x, y, z) => {
  console.log('setTimeout1', a.v, x, y, z);
}, 0, a, 123, 456);
console.log('t1', t1, t1.id);
a.v++;

const t2 = setTimeout((...args) => {
  console.log('setTimeout2', ...args2);
}, 0);
console.log('t2', t2, t2.id);

const t3 = setTimeout((...args) => {
  console.log('setTimeout3', ...args);
}, 1000);
console.log('t3', t3, t3.id);

const t4 = setTimeout((...args) => {
  console.log('setTimeout4', ...args);
}, 1000);
console.log('t4', t4, t4.id);
clearTimeout();
clearTimeout(t4);

let counter = 0;
const t5 = setInterval((...args) => {
  counter++;
  console.log('setInterval5', counter, ...args);
  if (counter > 5) {
    clearInterval(t5);
  }
}, 100);
console.log('t5', t5, t5.id);
