'use strict';

const fs = require('fs');
const path = require('path');
const moduleApi = require('node:module');

const projectRoot = path.resolve(__dirname, '../../..');
const tmpRoot = path.join(projectRoot, 'target', 'tmp');
const cacheDir = path.join(tmpRoot, 'module-compile-cache');

function ensure(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

fs.rmSync(cacheDir, { recursive: true, force: true });

const status = moduleApi.enableCompileCache({ directory: cacheDir, portable: false });
ensure(status.status === moduleApi.constants.compileCacheStatus.ENABLED, 'compile cache should enable');
ensure(status.directory === cacheDir, 'compile cache directory should match input');
ensure(status.portable === false, 'portable flag should reflect options');
ensure(fs.existsSync(path.join(cacheDir, 'version.txt')), 'version file should be created');

const repeat = moduleApi.enableCompileCache(cacheDir);
ensure(repeat.status === moduleApi.constants.compileCacheStatus.ALREADY_ENABLED,
       'enabling twice should report already enabled');

const reportedDir = moduleApi.getCompileCacheDir();
ensure(reportedDir === cacheDir, 'getCompileCacheDir should return configured path');

const flushResult = moduleApi.flushCompileCache();
ensure(flushResult === 0, 'flushCompileCache should return 0');

