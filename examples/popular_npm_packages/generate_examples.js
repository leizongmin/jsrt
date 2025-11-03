'use strict';

const fs = require('fs');
const path = require('path');

const examples = [
  {
    module: 'lodash',
    filename: 'test_lodash.js',
    content: `
const _ = require('lodash');

const grouped = _.groupBy([1, 2, 3, 4], (n) => (n % 2 ? 'odd' : 'even'));
console.log('Lodash groupBy result:', grouped);
`,
  },
  {
    module: 'chalk',
    filename: 'test_chalk.js',
    content: `
const chalk = require('chalk');

console.log(chalk.green('Chalk makes this line green!'));
console.log(chalk.bold.bgMagenta('Bold magenta text'));
`,
  },
  {
    module: 'request',
    filename: 'test_request.js',
    content: `
const request = require('request');

request.get('https://httpbin.org/get', { timeout: 3000 }, (err, res, body) => {
  if (err) {
    console.error('Request error:', err.message);
    return;
  }
  console.log('Request status:', res.statusCode);
  console.log('Body length:', body.length);
});
`,
  },
  {
    module: 'commander',
    filename: 'test_commander.js',
    content: `
const { Command } = require('commander');

const program = new Command();
program
  .name('test-commander')
  .option('-n, --name <name>', 'name', 'JSRT')
  .option('-v, --verbose', 'verbose flag');

program.parse(['node', 'test_commander.js', '--name', 'Runtime', '--verbose']);
console.log('Commander parsed options:', program.opts());
`,
  },
  {
    module: 'react',
    filename: 'test_react.js',
    content: `
const React = require('react');

const element = React.createElement('div', { className: 'greeting' }, 'Hello React');
console.log('React element type:', element.type);
console.log('React element props:', element.props);
`,
  },
  {
    module: 'express',
    filename: 'test_express.js',
    content: `
const express = require('express');

const app = express();
app.get('/ping', (_req, res) => res.send('pong'));

const server = app.listen(0, () => {
  const { port } = server.address();
  console.log('Express started on port', port);
  server.close();
});
`,
  },
  {
    module: 'debug',
    filename: 'test_debug.js',
    content: `
const debug = require('debug');

const log = debug('jsrt:test');
debug.enable('jsrt:*');
log('Debug logging enabled');
`,
  },
  {
    module: 'async',
    filename: 'test_async.js',
    content: `
const asyncLib = require('async');

asyncLib.parallel(
  {
    left(cb) {
      setTimeout(() => cb(null, 'L'), 20);
    },
    right(cb) {
      setTimeout(() => cb(null, 'R'), 10);
    },
  },
  (err, results) => {
    if (err) {
      console.error(err);
      return;
    }
    console.log('async.parallel results:', results);
  }
);
`,
  },
  {
    module: 'fs-extra',
    filename: 'test_fs_extra.js',
    content: `
const path = require('path');
const fs = require('fs-extra');

const tmpDir = path.join(__dirname, '..', '..', 'target', 'tmp', 'fs-extra');
fs.ensureDirSync(tmpDir);
const file = path.join(tmpDir, 'demo.txt');
fs.writeFileSync(file, 'fs-extra in action');
console.log('fs-extra read:', fs.readFileSync(file, 'utf8'));
`,
  },
  {
    module: 'moment',
    filename: 'test_moment.js',
    content: `
const moment = require('moment');

const now = moment();
console.log('Moment ISO string:', now.toISOString());
console.log('Three days ago:', now.clone().subtract(3, 'days').fromNow());
`,
  },
  {
    module: 'prop-types',
    filename: 'test_prop_types.js',
    content: `
const PropTypes = require('prop-types');

const props = { name: 'JSRT', active: true };
PropTypes.checkPropTypes(
  { name: PropTypes.string.isRequired, active: PropTypes.bool },
  props,
  'prop',
  'PropTypesDemo'
);
console.log('prop-types validation passed');
`,
  },
  {
    module: 'react-dom',
    filename: 'test_react_dom.js',
    content: `
const React = require('react');
const ReactDOMServer = require('react-dom/server');

const element = React.createElement('span', null, 'SSR ready');
const html = ReactDOMServer.renderToString(element);
console.log('ReactDOMServer output:', html);
`,
  },
  {
    module: 'bluebird',
    filename: 'test_bluebird.js',
    content: `
const PromiseBluebird = require('bluebird');

PromiseBluebird.map([1, 2, 3], (n) => n * 2).then((values) => {
  console.log('Bluebird map values:', values);
});
`,
  },
  {
    module: 'underscore',
    filename: 'test_underscore.js',
    content: `
const _ = require('underscore');

const result = _.shuffle([1, 2, 3, 4]);
console.log('Underscore shuffle result:', result);
`,
  },
  {
    module: 'vue',
    filename: 'test_vue.js',
    content: `
const Vue = require('vue');

const vm = new Vue({
  data: () => ({ message: 'Hello from Vue' }),
});
console.log('Vue reactive message:', vm.$data.message);
`,
  },
  {
    module: 'axios',
    filename: 'test_axios.js',
    content: `
const axios = require('axios');

const client = axios.create({
  adapter: async (config) => ({
    data: { url: config.url, method: config.method },
    status: 200,
    statusText: 'OK',
    headers: {},
    config,
    request: {},
  }),
});

client.get('https://example.com').then((response) => {
  console.log('Axios custom adapter URL:', response.data.url);
});
`,
  },
  {
    module: 'tslib',
    filename: 'test_tslib.js',
    content: `
const { __assign } = require('tslib');

const combined = __assign({ a: 1 }, { b: 2 });
console.log('tslib __assign result:', combined);
`,
  },
  {
    module: 'mkdirp',
    filename: 'test_mkdirp.js',
    content: `
const path = require('path');
const mkdirp = require('mkdirp');

const dir = path.join(__dirname, '..', '..', 'target', 'tmp', 'mkdirp', 'nested');
mkdirp.sync(dir);
console.log('mkdirp created directory:', dir);
`,
  },
  {
    module: 'glob',
    filename: 'test_glob.js',
    content: `
const glob = require('glob');

const matches = glob.sync('test_*.js', { cwd: __dirname });
console.log('glob matched files:', matches.length);
`,
  },
  {
    module: 'yargs',
    filename: 'test_yargs.js',
    content: `
const yargs = require('yargs/yargs');
const { hideBin } = require('yargs/helpers');

const argv = yargs(hideBin(['node', 'script', '--count', '3']))
  .option('count', { type: 'number', demandOption: true })
  .parse();

console.log('yargs count option:', argv.count);
`,
  },
  {
    module: 'colors',
    filename: 'test_colors.js',
    content: `
require('colors');

console.log('Colors adds styling to this string'.rainbow);
`,
  },
  {
    module: 'inquirer',
    filename: 'test_inquirer.js',
    content: `
const inquirer = require('inquirer');
const { Readable, Writable } = require('stream');

async function main() {
  const input = Readable.from(['JSRT\\n']);
  const output = new Writable({ write(_chunk, _enc, cb) { cb(); } });
  const prompt = inquirer.createPromptModule({ input, output });
  const answers = await prompt([{ type: 'input', name: 'name', message: 'Name?' }]);
  console.log('Inquirer answer:', answers.name);
}

main();
`,
  },
  {
    module: 'webpack',
    filename: 'test_webpack.js',
    content: `
const fs = require('fs');
const path = require('path');
const webpack = require('webpack');

const outDir = path.join(__dirname, '..', '..', 'target', 'tmp', 'webpack');
fs.mkdirSync(outDir, { recursive: true });

const compiler = webpack({
  mode: 'development',
  entry: path.join(__dirname, 'test_lodash.js'),
  output: {
    path: outDir,
    filename: 'bundle.js',
  },
});

compiler.run((err, stats) => {
  if (err) {
    console.error('Webpack error:', err.message);
  } else {
    console.log('Webpack assets:', stats.toJson({ assets: true }).assets.map((a) => a.name));
  }
  compiler.close(() => {});
});
`,
  },
  {
    module: 'uuid',
    filename: 'test_uuid.js',
    content: `
const { v4: uuidv4 } = require('uuid');

console.log('UUID v4:', uuidv4());
`,
  },
  {
    module: 'classnames',
    filename: 'test_classnames.js',
    content: `
const classNames = require('classnames');

const result = classNames('btn', { 'btn-primary': true, disabled: false });
console.log('classnames output:', result);
`,
  },
  {
    module: 'minimist',
    filename: 'test_minimist.js',
    content: `
const minimist = require('minimist');

const parsed = minimist(['--port', '8080', '--flag']);
console.log('minimist parsed object:', parsed);
`,
  },
  {
    module: 'body-parser',
    filename: 'test_body_parser.js',
    content: `
const express = require('express');
const bodyParser = require('body-parser');

const app = express();
app.use(bodyParser.json());
app.post('/echo', (req, res) => res.json(req.body));

const server = app.listen(0, () => {
  console.log('body-parser middleware attached');
  server.close();
});
`,
  },
  {
    module: 'rxjs',
    filename: 'test_rxjs.js',
    content: `
const { of } = require('rxjs');
const { map } = require('rxjs/operators');

of(1, 2, 3)
  .pipe(map((n) => n * 10))
  .subscribe((value) => console.log('RxJS emitted:', value));
`,
  },
  {
    module: 'babel-runtime',
    filename: 'test_babel_runtime.js',
    content: `
const regeneratorRuntime = require('babel-runtime/regenerator');

function* generatorDemo() {
  yield 'hello';
  yield 'runtime';
}

console.log('babel-runtime generator output:', Array.from(generatorDemo()));
console.log('regeneratorRuntime object:', typeof regeneratorRuntime === 'object');
`,
  },
  {
    module: 'jquery',
    filename: 'test_jquery.js',
    content: `
const { JSDOM } = require('jsdom');
const dom = new JSDOM('<!doctype html><p>Hello</p>');
const $ = require('jquery')(dom.window);

$('p').text('jQuery updated content');
console.log('jQuery text:', $('p').text());
`,
  },
  {
    module: 'yeoman-generator',
    filename: 'test_yeoman_generator.js',
    content: `
const Generator = require('yeoman-generator');

class DemoGenerator extends Generator {
  writing() {
    this.log('Yeoman generator writing phase executed');
  }
}

const env = Generator.Environment.createEnv();
env.registerStub(DemoGenerator, 'demo:app');
env.run('demo:app');
`,
  },
  {
    module: 'through2',
    filename: 'test_through2.js',
    content: `
const through2 = require('through2');

const stream = through2(function (chunk, _enc, callback) {
  this.push(chunk.toString().toUpperCase());
  callback();
});

stream.on('data', (data) => console.log('through2 output:', data.toString()));
stream.end('hello');
`,
  },
  {
    module: 'babel-core',
    filename: 'test_babel_core.js',
    content: `
const babel = require('babel-core');

const result = babel.transform('const square = (n) => n * n;', { presets: ['es2015'] });
console.log('babel-core transformed code:', result.code);
`,
  },
  {
    module: 'core-js',
    filename: 'test_core_js.js',
    content: `
require('core-js/es/promise');

Promise.resolve('polyfill').then((value) => console.log('core-js Promise resolved:', value));
`,
  },
  {
    module: 'semver',
    filename: 'test_semver.js',
    content: `
const semver = require('semver');

console.log('semver satisfies:', semver.satisfies('1.5.0', '^1.0.0'));
console.log('semver compare:', semver.gt('2.0.0', '1.0.0'));
`,
  },
  {
    module: 'babel-loader',
    filename: 'test_babel_loader.js',
    content: `
const babelLoader = require('babel-loader');

console.log('babel-loader export type:', typeof babelLoader);
`,
  },
  {
    module: 'cheerio',
    filename: 'test_cheerio.js',
    content: `
const cheerio = require('cheerio');

const $ = cheerio.load('<ul><li>one</li><li>two</li></ul>');
console.log('cheerio li count:', $('li').length);
`,
  },
  {
    module: 'rimraf',
    filename: 'test_rimraf.js',
    content: `
const fs = require('fs');
const path = require('path');
const rimraf = require('rimraf');

const dir = path.join(__dirname, '..', '..', 'target', 'tmp', 'rimraf-demo');
fs.mkdirSync(dir, { recursive: true });
fs.writeFileSync(path.join(dir, 'file.txt'), 'cleanup');
rimraf.sync(dir);
console.log('rimraf removed directory:', !fs.existsSync(dir));
`,
  },
  {
    module: 'q',
    filename: 'test_q.js',
    content: `
const Q = require('q');

Q.all([Q.resolve(1), Q.resolve(2)]).then((values) => {
  console.log('Q promises resolved:', values);
});
`,
  },
  {
    module: 'eslint',
    filename: 'test_eslint.js',
    content: `
const { ESLint } = require('eslint');

async function run() {
  const eslint = new ESLint({ useEslintrc: false, baseConfig: { extends: 'eslint:recommended' } });
  const results = await eslint.lintText('const foo = 1;\\n');
  console.log('ESLint messages:', results[0].messages.length);
}

run();
`,
  },
  {
    module: 'css-loader',
    filename: 'test_css_loader.js',
    content: `
const cssLoader = require('css-loader');

console.log('css-loader export type:', typeof cssLoader);
`,
  },
  {
    module: 'shelljs',
    filename: 'test_shelljs.js',
    content: `
const shell = require('shelljs');

const output = shell.echo('ShellJS demo');
console.log('shell.echo output:', output.stdout.trim());
`,
  },
  {
    module: 'dotenv',
    filename: 'test_dotenv.js',
    content: `
const fs = require('fs');
const path = require('path');
const dotenv = require('dotenv');

const envFile = path.join(__dirname, '..', '..', 'target', 'tmp', 'dotenv.env');
fs.writeFileSync(envFile, 'TOKEN=dotenv-demo');
const parsed = dotenv.config({ path: envFile });
console.log('dotenv parsed TOKEN:', parsed.parsed.TOKEN);
`,
  },
  {
    module: 'typescript',
    filename: 'test_typescript.js',
    content: `
const ts = require('typescript');

const source = 'const answer: number = 42;';
const result = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.CommonJS } });
console.log('TypeScript transpiled output:', result.outputText.trim());
`,
  },
  {
    module: '@types/node',
    filename: 'test_types_node.js',
    content: `
/// <reference types="@types/node" />

const assert = require('assert');

assert.strictEqual(typeof process.cwd, 'function');
console.log('@types/node ambient declarations available');
`,
  },
  {
    module: '@angular/core',
    filename: 'test_angular_core.js',
    content: `
const core = require('@angular/core');

console.log('Angular core version:', core.VERSION.full);
`,
  },
  {
    module: 'js-yaml',
    filename: 'test_js_yaml.js',
    content: `
const yaml = require('js-yaml');

const doc = yaml.load('greeting: hello');
console.log('js-yaml parsed greeting:', doc.greeting);
`,
  },
  {
    module: 'style-loader',
    filename: 'test_style_loader.js',
    content: `
const styleLoader = require('style-loader');

console.log('style-loader export keys:', Object.keys(styleLoader));
`,
  },
  {
    module: 'winston',
    filename: 'test_winston.js',
    content: `
const winston = require('winston');

const logger = winston.createLogger({
  transports: [new winston.transports.Console()],
});

logger.info('Winston logger is working');
`,
  },
  {
    module: '@angular/common',
    filename: 'test_angular_common.js',
    content: `
const common = require('@angular/common');

const pipe = new common.DatePipe('en-US');
console.log('Angular DatePipe output:', pipe.transform(new Date(0)));
`,
  },
  {
    module: 'redux',
    filename: 'test_redux.js',
    content: `
const { createStore } = require('redux');

function reducer(state = 0, action) {
  if (action.type === 'increment') {
    return state + 1;
  }
  return state;
}

const store = createStore(reducer);
store.dispatch({ type: 'increment' });
console.log('Redux state:', store.getState());
`,
  },
  {
    module: 'object-assign',
    filename: 'test_object_assign.js',
    content: `
const objectAssign = require('object-assign');

const merged = objectAssign({ a: 1 }, { b: 2 });
console.log('object-assign result:', merged);
`,
  },
  {
    module: 'zone.js',
    filename: 'test_zone_js.js',
    content: `
require('zone.js/dist/zone-node');

Zone.current.fork({ name: 'demo-zone' }).run(() => {
  console.log('zone.js current zone:', Zone.current.name);
});
`,
  },
  {
    module: 'babel-eslint',
    filename: 'test_babel_eslint.js',
    content: `
const babelEslint = require('babel-eslint');

const ast = babelEslint.parse('const foo = () => 1;', { sourceType: 'module' });
console.log('babel-eslint AST type:', ast.type);
`,
  },
  {
    module: 'gulp',
    filename: 'test_gulp.js',
    content: `
const path = require('path');
const { src, dest } = require('gulp');
const { Transform } = require('stream');

const uppercase = new Transform({
  transform(chunk, _enc, callback) {
    callback(null, chunk.toString().toUpperCase());
  },
});

src(__filename)
  .pipe(uppercase)
  .pipe(dest(path.join(__dirname, '..', '..', 'target', 'tmp', 'gulp-output')))
  .on('finish', () => console.log('gulp stream finished'));
`,
  },
  {
    module: 'gulp-util',
    filename: 'test_gulp_util.js',
    content: `
const gutil = require('gulp-util');

console.log('gulp-util colored text:', gutil.colors.yellow('hello'));
`,
  },
  {
    module: 'file-loader',
    filename: 'test_file_loader.js',
    content: `
const fileLoader = require('file-loader');

console.log('file-loader export type:', typeof fileLoader);
`,
  },
  {
    module: 'ora',
    filename: 'test_ora.js',
    content: `
const ora = require('ora');

const spinner = ora('Testing ora spinner').start();
setTimeout(() => spinner.succeed('Ora spinner success'), 100);
`,
  },
  {
    module: 'node-fetch',
    filename: 'test_node_fetch.js',
    content: `
const fetch = require('node-fetch');

fetch('https://httpbin.org/get')
  .then((res) => res.json())
  .then((json) => console.log('node-fetch URL:', json.url))
  .catch((err) => console.error('node-fetch error:', err.message));
`,
  },
  {
    module: '@angular/platform-browser',
    filename: 'test_angular_platform_browser.js',
    content: `
const platformBrowser = require('@angular/platform-browser');

console.log('Angular platform-browser DomSanitizer:', typeof platformBrowser.DomSanitizer);
`,
  },
  {
    module: '@babel/runtime',
    filename: 'test_babel_runtime_pkg.js',
    content: `
const asyncToGenerator = require('@babel/runtime/helpers/asyncToGenerator');

const run = asyncToGenerator(function* () {
  return 'async result';
});

run().then((value) => console.log('@babel/runtime async helper result:', value));
`,
  },
  {
    module: 'handlebars',
    filename: 'test_handlebars.js',
    content: `
const Handlebars = require('handlebars');

const template = Handlebars.compile('Hello {{name}}!');
console.log('Handlebars rendered output:', template({ name: 'JSRT' }));
`,
  },
  {
    module: 'eslint-plugin-import',
    filename: 'test_eslint_plugin_import.js',
    content: `
const { ESLint } = require('eslint');

async function main() {
  const eslint = new ESLint({
    useEslintrc: false,
    baseConfig: {
      parserOptions: { ecmaVersion: 2020, sourceType: 'module' },
      plugins: ['import'],
      rules: { 'import/no-unresolved': 'error' },
    },
    plugins: { import: require('eslint-plugin-import') },
  });
  const results = await eslint.lintText('import fs from "fs";\\nconsole.log(fs);');
  console.log('eslint-plugin-import messages:', results[0].messages.length);
}

main();
`,
  },
  {
    module: '@angular/compiler',
    filename: 'test_angular_compiler.js',
    content: `
const compiler = require('@angular/compiler');

console.log('Angular compiler version:', compiler.VERSION.full);
`,
  },
  {
    module: 'eslint-plugin-react',
    filename: 'test_eslint_plugin_react.js',
    content: `
const { ESLint } = require('eslint');

async function run() {
  const eslint = new ESLint({
    useEslintrc: false,
    baseConfig: {
      parserOptions: { ecmaVersion: 2020, sourceType: 'module', ecmaFeatures: { jsx: true } },
      plugins: ['react'],
      rules: { 'react/jsx-uses-react': 'error', 'react/jsx-uses-vars': 'error' },
    },
    plugins: { react: require('eslint-plugin-react') },
  });
  const results = await eslint.lintText('const React = require("react");\\nconst App = () => <div/>;\\nmodule.exports = App;');
  console.log('eslint-plugin-react messages:', results[0].messages.length);
}

run();
`,
  },
  {
    module: 'aws-sdk',
    filename: 'test_aws_sdk.js',
    content: `
const AWS = require('aws-sdk');

const s3 = new AWS.S3({
  region: 'us-east-1',
  credentials: { accessKeyId: 'AKIAFAKE', secretAccessKey: 'SECRET' },
});
console.log('aws-sdk S3 methods sample:', Object.keys(s3).slice(0, 3));
`,
  },
  {
    module: 'yosay',
    filename: 'test_yosay.js',
    content: `
const yosay = require('yosay');

console.log(yosay('JSRT welcomes yosay!'));
`,
  },
  {
    module: 'url-loader',
    filename: 'test_url_loader.js',
    content: `
const urlLoader = require('url-loader');

console.log('url-loader export type:', typeof urlLoader);
`,
  },
  {
    module: '@angular/forms',
    filename: 'test_angular_forms.js',
    content: `
const forms = require('@angular/forms');

const control = new forms.FormControl('works');
console.log('Angular FormControl value:', control.value);
`,
  },
  {
    module: 'webpack-dev-server',
    filename: 'test_webpack_dev_server.js',
    content: `
const path = require('path');
const webpack = require('webpack');
const WebpackDevServer = require('webpack-dev-server');

const compiler = webpack({
  mode: 'development',
  entry: path.join(__dirname, 'test_classnames.js'),
  output: {
    path: path.join(__dirname, '..', '..', 'target', 'tmp', 'webpack-dev-server'),
    filename: 'bundle.js',
  },
});

const server = new WebpackDevServer({ port: 0, hot: false }, compiler);
server.start().then(() => {
  console.log('WebpackDevServer started');
  server.stop();
});
`,
  },
  {
    module: '@angular/platform-browser-dynamic',
    filename: 'test_angular_platform_browser_dynamic.js',
    content: `
const platformBrowserDynamic = require('@angular/platform-browser-dynamic');

console.log(
  'Angular platform-browser-dynamic bootstrapModule type:',
  typeof platformBrowserDynamic.platformBrowserDynamic
);
`,
  },
  {
    module: 'mocha',
    filename: 'test_mocha.js',
    content: `
const Mocha = require('mocha');

const mocha = new Mocha();
mocha.suite.addTest(
  new Mocha.Test('addition works', function () {
    if (1 + 1 !== 2) throw new Error('Math broke');
  })
);

mocha.run(() => console.log('Mocha test run complete'));
`,
  },
  {
    module: 'html-webpack-plugin',
    filename: 'test_html_webpack_plugin.js',
    content: `
const HtmlWebpackPlugin = require('html-webpack-plugin');

const plugin = new HtmlWebpackPlugin({ title: 'JSRT Demo' });
console.log('html-webpack-plugin option keys:', Object.keys(plugin.options));
`,
  },
  {
    module: 'socket.io',
    filename: 'test_socket_io.js',
    content: `
const http = require('http');
const { Server } = require('socket.io');

const server = http.createServer();
const io = new Server(server);
io.on('connection', (socket) => {
  console.log('socket.io client connected');
  socket.emit('greeting', 'hello from socket.io');
  socket.disconnect(true);
});
server.listen(0, () => {
  const port = server.address().port;
  console.log('socket.io server listening on', port);
  io.close();
  server.close();
});
`,
  },
  {
    module: 'ws',
    filename: 'test_ws.js',
    content: `
const WebSocket = require('ws');

const server = new WebSocket.Server({ port: 0 }, () => {
  const { port } = server.address();
  const client = new WebSocket('ws://localhost:' + port);
  client.on('open', () => client.send('ping'));
  client.on('message', (message) => {
    console.log('ws client received:', message.toString());
    client.close();
    server.close();
  });
});

server.on('connection', (socket) => {
  socket.on('message', (message) => socket.send(message.toString().toUpperCase()));
});
`,
  },
  {
    module: 'babel-preset-es2015',
    filename: 'test_babel_preset_es2015.js',
    content: `
const babel = require('babel-core');

const transformed = babel.transform('let sum = (a, b) => a + b;', { presets: ['es2015'] });
console.log('babel-preset-es2015 output:', transformed.code);
`,
  },
  {
    module: 'postcss-loader',
    filename: 'test_postcss_loader.js',
    content: `
const postcssLoader = require('postcss-loader');

console.log('postcss-loader export type:', typeof postcssLoader);
`,
  },
  {
    module: 'node-sass',
    filename: 'test_node_sass.js',
    content: `
const sass = require('node-sass');

const result = sass.renderSync({ data: '$color: #333; body { color: $color; }' });
console.log('node-sass compiled css:', result.css.toString().trim());
`,
  },
  {
    module: 'ember-cli-babel',
    filename: 'test_ember_cli_babel.js',
    content: `
const emberCliBabel = require('ember-cli-babel');

console.log('ember-cli-babel export keys:', Object.keys(emberCliBabel));
`,
  },
  {
    module: 'babel-polyfill',
    filename: 'test_babel_polyfill.js',
    content: `
require('babel-polyfill');

new Promise((resolve) => resolve('polyfilled')).then((value) =>
  console.log('babel-polyfill Promise resolved:', value)
);
`,
  },
  {
    module: '@angular/router',
    filename: 'test_angular_router.js',
    content: `
const router = require('@angular/router');

const routes = [{ path: '', redirectTo: 'home', pathMatch: 'full' }];
console.log('Angular router RouterModule:', typeof router.RouterModule.forRoot(routes));
`,
  },
  {
    module: 'ramda',
    filename: 'test_ramda.js',
    content: `
const R = require('ramda');

const doubled = R.map((n) => n * 2, [1, 2, 3]);
console.log('Ramda map result:', doubled);
`,
  },
  {
    module: 'react-redux',
    filename: 'test_react_redux.js',
    content: `
const React = require('react');
const { Provider, connect } = require('react-redux');
const { createStore } = require('redux');
const ReactDOMServer = require('react-dom/server');

const reducer = (state = { value: 1 }) => state;
const store = createStore(reducer);

const View = ({ value }) => React.createElement('span', null, value);
const ConnectedView = connect((state) => state)(View);

const html = ReactDOMServer.renderToString(
  React.createElement(Provider, { store }, React.createElement(ConnectedView))
);

console.log('react-redux rendered markup:', html);
`,
  },
  {
    module: '@babel/core',
    filename: 'test_babel_core_latest.js',
    content: `
const babel = require('@babel/core');

const output = babel.transformSync('const add = (a, b) => a + b;', {});
console.log('@babel/core output code:', output.code);
`,
  },
  {
    module: '@angular/http',
    filename: 'test_angular_http.js',
    content: `
const http = require('@angular/http');

console.log('Angular HttpModule name:', http.HttpModule.name);
`,
  },
  {
    module: 'ejs',
    filename: 'test_ejs.js',
    content: `
const ejs = require('ejs');

const html = ejs.render('<p>Hello <%= name %></p>', { name: 'JSRT' });
console.log('EJS rendered HTML:', html);
`,
  },
  {
    module: 'coffee-script',
    filename: 'test_coffee_script.js',
    content: `
const coffee = require('coffee-script');

const js = coffee.compile('square = (x) -> x * x');
console.log('CoffeeScript compiled JS:', js.trim());
`,
  },
  {
    module: 'superagent',
    filename: 'test_superagent.js',
    content: `
const superagent = require('superagent');

superagent
  .get('https://httpbin.org/get')
  .then((res) => console.log('superagent status:', res.status))
  .catch((err) => console.error('superagent error:', err.message));
`,
  },
  {
    module: 'request-promise',
    filename: 'test_request_promise.js',
    content: `
const rp = require('request-promise');

rp('https://httpbin.org/uuid')
  .then((body) => console.log('request-promise body length:', body.length))
  .catch((err) => console.error('request-promise error:', err.message));
`,
  },
  {
    module: 'autoprefixer',
    filename: 'test_autoprefixer.js',
    content: `
const autoprefixer = require('autoprefixer');
const postcss = require('postcss');

postcss([autoprefixer])
  .process('a { display: flex }', { from: undefined })
  .then((result) => console.log('autoprefixer output:', result.css));
`,
  },
  {
    module: 'path',
    filename: 'test_path.js',
    content: `
const path = require('path');

console.log('path join example:', path.join('/home', 'user', 'file.txt'));
`,
  },
  {
    module: 'mongodb',
    filename: 'test_mongodb.js',
    content: `
const { MongoClient } = require('mongodb');

const client = new MongoClient('mongodb://localhost:27017', { serverSelectionTimeoutMS: 1000 });
client.connect().then(
  () => {
    console.log('MongoDB connected (requires local server)');
    return client.close();
  },
  (err) => console.error('MongoDB connection failed (expected if no server):', err.message)
);
`,
  },
  {
    module: 'chai',
    filename: 'test_chai.js',
    content: `
const { expect } = require('chai');

expect(2 + 2).to.equal(4);
console.log('Chai expectation passed');
`,
  },
  {
    module: 'mongoose',
    filename: 'test_mongoose.js',
    content: `
const mongoose = require('mongoose');

const userSchema = new mongoose.Schema({ name: String });
console.log('Mongoose model name:', mongoose.model('User', userSchema).modelName);
`,
  },
  {
    module: 'xml2js',
    filename: 'test_xml2js.js',
    content: `
const xml2js = require('xml2js');

xml2js.parseString('<user><name>jsrt</name></user>', (err, result) => {
  if (err) throw err;
  console.log('xml2js parsed name:', result.user.name[0]);
});
`,
  },
  {
    module: 'bootstrap',
    filename: 'test_bootstrap.js',
    content: `
require('bootstrap');
const version = require('bootstrap/package.json').version;
console.log('Bootstrap JS loaded, version:', version);
`,
  },
  {
    module: 'jest',
    filename: 'test_jest.js',
    content: `
const { runCLI } = require('jest');

runCLI(
  {
    runInBand: true,
    silent: true,
    testMatch: [],
    passWithNoTests: true,
  },
  [process.cwd()]
).then(() => console.log('Jest CLI initialized'));
`,
  },
  {
    module: 'sass-loader',
    filename: 'test_sass_loader.js',
    content: `
const sassLoader = require('sass-loader');

console.log('sass-loader export keys:', Object.keys(sassLoader));
`,
  },
  {
    module: 'redis',
    filename: 'test_redis.js',
    content: `
const redis = require('redis');

const client = redis.createClient({ url: 'redis://localhost:6379' });
client.on('error', (err) => console.error('Redis error (expected if server missing):', err.message));
client.connect().then(() => {
  console.log('Redis connected');
  return client.quit();
});
`,
  },
  {
    module: 'vue-router',
    filename: 'test_vue_router.js',
    content: `
const Vue = require('vue');
const VueRouter = require('vue-router');

Vue.use(VueRouter);
const router = new VueRouter({
  routes: [{ path: '/', component: { template: '<div>Home</div>' } }],
});

console.log('VueRouter current route path:', router.currentRoute.fullPath);
`,
  },
];

for (const { module, filename, content } of examples) {
  const header = `// Compatibility smoke test for "${module}"\n// Auto-generated by generate_examples.js\n\n`;
  const filePath = path.join(__dirname, filename);
  fs.writeFileSync(filePath, header + content.trimStart());
}

console.log(`Generated ${examples.length} compatibility examples.`);
