// Keywords in strings should be ignored
const text = "This file mentions import and export but they're in strings";
const code = 'require("test")';
const template = `module.exports = value`;

module.exports = { text, code, template };
