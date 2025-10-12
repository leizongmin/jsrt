// Wrapper module that tests path resolution from within a module context
const relativeHelper = require('./test_module_relative_helper.js');
const parentHelper = require('../test_module_parent_helper.js');

module.exports = {
  relativeValue: relativeHelper.value,
  parentValue: parentHelper.value,
};
