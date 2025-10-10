// Internal zlib stream implementation using Transform streams
// This file implements stream-based compression/decompression by extending Transform
// from node:stream and wrapping the native zlib compression methods.
//
// Architecture Decision:
// - Streams are implemented in JavaScript for maximum code reuse
// - Extends existing Transform class from node:stream (battle-tested)
// - Wraps native C compression methods for actual compression logic
// - Separates concerns: JS for stream interface, C for compression
// - Simpler, more maintainable than complex C/JS integration
//
// Usage:
//   const zlib = require('node:zlib');
//   const { createGzip } = require('./path/to/zlib_streams.js');
//   const gzip = createGzip({ level: 9 });
//   inputStream.pipe(gzip).pipe(outputStream);

const { Transform } = require('node:stream');
const zlib = require('node:zlib');

// Base class for all zlib Transform streams
// Implements the Transform interface by wrapping native compression methods
class ZlibBase extends Transform {
  constructor(opts, mode) {
    super(opts);
    this._mode = mode;
    this._opts = opts || {};
    this._hadError = false;
    this._chunks = [];  // Accumulate chunks for better compression
    this._chunkSize = this._opts.chunkSize || 16384;
  }

  // Common transform implementation for all zlib streams
  // Processes each chunk through the appropriate native compression method
  _transform(chunk, encoding, callback) {
    if (this._hadError) {
      return callback();
    }

    try {
      // Convert chunk to Uint8Array if needed
      const input = chunk instanceof Uint8Array ? chunk : 
                    Buffer.isBuffer(chunk) ? new Uint8Array(chunk) :
                    new Uint8Array(chunk.buffer || chunk);
      
      // Accumulate chunks to compress in larger batches for better efficiency
      this._chunks.push(input);
      const totalSize = this._chunks.reduce((sum, c) => sum + c.length, 0);
      
      // Only process when we have enough data or this is a small chunk
      if (totalSize >= this._chunkSize || input.length < 1024) {
        this._processChunks(callback);
      } else {
        // Accumulate more data
        callback();
      }
    } catch (err) {
      this._hadError = true;
      callback(err);
    }
  }

  // Process accumulated chunks
  _processChunks(callback) {
    if (this._chunks.length === 0) {
      return callback();
    }

    try {
      // Combine chunks
      const totalSize = this._chunks.reduce((sum, c) => sum + c.length, 0);
      const combined = new Uint8Array(totalSize);
      let offset = 0;
      for (const chunk of this._chunks) {
        combined.set(chunk, offset);
        offset += chunk.length;
      }
      this._chunks = [];

      // Compress/decompress using native method
      let result;
      switch (this._mode) {
        case 'gzip':
          result = zlib.gzipSync(combined, this._opts);
          break;
        case 'gunzip':
          result = zlib.gunzipSync(combined, this._opts);
          break;
        case 'deflate':
          result = zlib.deflateSync(combined, this._opts);
          break;
        case 'inflate':
          result = zlib.inflateSync(combined, this._opts);
          break;
        case 'deflateRaw':
          result = zlib.deflateRawSync(combined, this._opts);
          break;
        case 'inflateRaw':
          result = zlib.inflateRawSync(combined, this._opts);
          break;
        case 'unzip':
          result = zlib.unzipSync(combined, this._opts);
          break;
        default:
          throw new Error('Unknown zlib mode: ' + this._mode);
      }
      
      // Push result to output
      this.push(result);
      callback();
    } catch (err) {
      this._hadError = true;
      callback(err);
    }
  }

  // Final flush - process any remaining chunks
  _flush(callback) {
    if (this._chunks.length > 0) {
      this._processChunks(callback);
    } else {
      callback();
    }
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

// Raw deflate compression stream (no headers)
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

// Auto-detecting decompression stream (handles both gzip and deflate)
class Unzip extends ZlibBase {
  constructor(opts) {
    super(opts, 'unzip');
  }
}

// Factory functions for creating stream instances
function createGzip(options) {
  return new Gzip(options);
}

function createGunzip(options) {
  return new Gunzip(options);
}

function createDeflate(options) {
  return new Deflate(options);
}

function createInflate(options) {
  return new Inflate(options);
}

function createDeflateRaw(options) {
  return new DeflateRaw(options);
}

function createInflateRaw(options) {
  return new InflateRaw(options);
}

function createUnzip(options) {
  return new Unzip(options);
}

// Export all stream classes and factory functions
module.exports = {
  // Classes
  Gzip,
  Gunzip,
  Deflate,
  Inflate,
  DeflateRaw,
  InflateRaw,
  Unzip,
  ZlibBase,
  
  // Factory functions
  createGzip,
  createGunzip,
  createDeflate,
  createInflate,
  createDeflateRaw,
  createInflateRaw,
  createUnzip
};
