// Internal zlib stream implementation using Transform streams
// This file extends the native zlib module with stream classes

const { Transform } = require('node:stream');
const zlib = require('node:zlib');

// Base class for zlib Transform streams
class ZlibBase extends Transform {
  constructor(opts, mode) {
    super(opts);
    this._mode = mode;
    this._opts = opts || {};
    this._hadError = false;
  }

  // Common transform implementation for all zlib streams
  _transform(chunk, encoding, callback) {
    if (this._hadError) {
      return callback();
    }

    try {
      const input = chunk instanceof Uint8Array ? chunk : new Uint8Array(chunk.buffer || chunk);
      let result;
      
      // Call appropriate sync method based on mode
      switch (this._mode) {
        case 'gzip':
          result = zlib.gzipSync(input, this._opts);
          break;
        case 'gunzip':
          result = zlib.gunzipSync(input, this._opts);
          break;
        case 'deflate':
          result = zlib.deflateSync(input, this._opts);
          break;
        case 'inflate':
          result = zlib.inflateSync(input, this._opts);
          break;
        case 'deflateRaw':
          result = zlib.deflateRawSync(input, this._opts);
          break;
        case 'inflateRaw':
          result = zlib.inflateRawSync(input, this._opts);
          break;
        case 'unzip':
          result = zlib.unzipSync(input, this._opts);
          break;
        default:
          return callback(new Error('Unknown zlib mode: ' + this._mode));
      }
      
      this.push(result);
      callback();
    } catch (err) {
      this._hadError = true;
      callback(err);
    }
  }

  _flush(callback) {
    // Finalization is already handled by sync methods
    callback();
  }
}

// Gzip compression stream
class Gzip extends ZlibBase {
  constructor(opts) {
    super(opts, 'gzip');
  }
}

// Gzip decompression stream
class Gunzip extends ZlibBase {
  constructor(opts) {
    super(opts, 'gunzip');
  }
}

// Deflate compression stream
class Deflate extends ZlibBase {
  constructor(opts) {
    super(opts, 'deflate');
  }
}

// Deflate decompression stream
class Inflate extends ZlibBase {
  constructor(opts) {
    super(opts, 'inflate');
  }
}

// Raw deflate compression stream
class DeflateRaw extends ZlibBase {
  constructor(opts) {
    super(opts, 'deflateRaw');
  }
}

// Raw deflate decompression stream
class InflateRaw extends ZlibBase {
  constructor(opts) {
    super(opts, 'inflateRaw');
  }
}

// Auto-detect decompression stream
class Unzip extends ZlibBase {
  constructor(opts) {
    super(opts, 'unzip');
  }
}

// Factory functions
function createGzip(opts) {
  return new Gzip(opts);
}

function createGunzip(opts) {
  return new Gunzip(opts);
}

function createDeflate(opts) {
  return new Deflate(opts);
}

function createInflate(opts) {
  return new Inflate(opts);
}

function createDeflateRaw(opts) {
  return new DeflateRaw(opts);
}

function createInflateRaw(opts) {
  return new InflateRaw(opts);
}

function createUnzip(opts) {
  return new Unzip(opts);
}

// Export stream classes and factory functions to the zlib module
zlib.Gzip = Gzip;
zlib.Gunzip = Gunzip;
zlib.Deflate = Deflate;
zlib.Inflate = Inflate;
zlib.DeflateRaw = DeflateRaw;
zlib.InflateRaw = InflateRaw;
zlib.Unzip = Unzip;

zlib.createGzip = createGzip;
zlib.createGunzip = createGunzip;
zlib.createDeflate = createDeflate;
zlib.createInflate = createInflate;
zlib.createDeflateRaw = createDeflateRaw;
zlib.createInflateRaw = createInflateRaw;
zlib.createUnzip = createUnzip;

// Return the enhanced module
module.exports = zlib;
