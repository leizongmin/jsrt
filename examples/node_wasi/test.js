const { readFile } = require('node:fs/promises');
const { WASI } = require('node:wasi');
const { argv, env } = require('node:process');

async function main() {
  console.log('Starting WASI example...');

  try {
    const wasmBuffer = await readFile(__dirname + '/demo.wasm');
    console.log('WASM file read successfully, size:', wasmBuffer.byteLength);

    const wasi = new WASI({
      version: 'preview1',
      args: argv,
      env,
      preopens: {
        '/local': process.cwd(),
      },
    });

    const wasm = await WebAssembly.compile(wasmBuffer);
    const instance = await WebAssembly.instantiate(
      wasm,
      wasi.getImportObject()
    );

    console.log('WASI instance created, starting...');
    wasi.start(instance);
    console.log('WASI execution completed.');
  } catch (error) {
    console.error('Error details:', {
      message: error.message,
      name: error.name,
      stack: error.stack,
      code: error.code,
    });
    process.exit(1);
  }
}

main();
