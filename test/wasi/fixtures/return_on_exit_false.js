'use strict';

const { WASI } = require('node:wasi');
const wasi = new WASI({});
const { wasi_snapshot_preview1: namespace } = wasi.getImportObject();

console.log('Invoking proc_exit(33) with returnOnExit=false');
namespace.proc_exit(33);

console.log('This line should never be reached.');
