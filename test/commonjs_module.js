// CommonJS test module
function greet(name) {
  return `Greetings, ${name}!`;
}

const data = { count: 100 };

module.exports = {
  greet,
  data
};