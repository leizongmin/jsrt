#include "build.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jsrt.h"
#include "runtime.h"
#include "util/file.h"

int BuildExecutable(const char* executable_path, const char* filename, const char* target) {
  printf("Building self-contained executable from %s...\n", filename);

  // Determine output filename
  char output_name[256];
  if (target) {
    strncpy(output_name, target, sizeof(output_name) - 1);
    output_name[sizeof(output_name) - 1] = '\0';
  } else {
    // Default: use input filename without extension
    const char* dot = strrchr(filename, '.');
    if (dot) {
      size_t len = dot - filename;
      len = len < sizeof(output_name) - 1 ? len : sizeof(output_name) - 1;
      strncpy(output_name, filename, len);
      output_name[len] = '\0';
    } else {
      strncpy(output_name, filename, sizeof(output_name) - 1);
      output_name[sizeof(output_name) - 1] = '\0';
    }
  }

  printf("Output target: %s\n", output_name);

  // Check if input file exists and read it
  JSRT_ReadFileResult input_file = JSRT_ReadFile(filename);
  if (input_file.error != JSRT_READ_FILE_OK) {
    fprintf(stderr, "Error: Cannot read input file '%s': %s\n", filename, JSRT_ReadFileErrorToString(input_file.error));
    return 1;
  }

  // Check if the file uses ES6 modules (import/export)
  if (JS_DetectModule(input_file.data, input_file.size)) {
    fprintf(stderr, "Error: ES6 modules (import/export) are not supported in self-contained executables.\n");
    fprintf(stderr, "       Please use CommonJS require() instead of ES6 import.\n");
    fprintf(stderr, "       Example: const process = require('std:process');\n");
    JSRT_ReadFileResultFree(&input_file);
    return 1;
  }

  // Step 1: Create a jsrt runtime to compile the JavaScript with all modules available
  printf("Compiling JavaScript to bytecode...\n");
  JSRT_Runtime* rt = JSRT_RuntimeNew();
  if (!rt) {
    fprintf(stderr, "Error: Failed to create runtime\n");
    JSRT_ReadFileResultFree(&input_file);
    return 1;
  }

  // Step 2: Compile JavaScript to bytecode using jsrt's runtime
  JSRT_CompileResult compile_result = JSRT_RuntimeCompileToBytecode(rt, filename, input_file.data, input_file.size);
  JSRT_ReadFileResultFree(&input_file);

  if (compile_result.error) {
    fprintf(stderr, "Error: Compilation failed: %s\n", compile_result.error);
    JSRT_CompileResultFree(&compile_result);
    JSRT_RuntimeFree(rt);
    return 1;
  }

  JSRT_RuntimeFree(rt);

  // Step 3: Write bytecode to temporary file
  char temp_bytecode_file[256];
  snprintf(temp_bytecode_file, sizeof(temp_bytecode_file), "/tmp/jsrt_build_%d.bin", (int)getpid());

  FILE* bytecode_file_fp = fopen(temp_bytecode_file, "wb");
  if (!bytecode_file_fp) {
    fprintf(stderr, "Error: Cannot create bytecode file\n");
    JSRT_CompileResultFree(&compile_result);
    return 1;
  }

  size_t bytes_written = fwrite(compile_result.data, 1, compile_result.size, bytecode_file_fp);
  fclose(bytecode_file_fp);

  if (bytes_written != compile_result.size) {
    fprintf(stderr, "Error: Failed to write bytecode file\n");
    JSRT_CompileResultFree(&compile_result);
    unlink(temp_bytecode_file);
    return 1;
  }

  size_t bytecode_size = compile_result.size;
  JSRT_CompileResultFree(&compile_result);

  // Step 4: Copy current jsrt executable as base
  printf("Creating self-contained executable...\n");

  // Open source and destination files
  FILE* src_file = fopen(executable_path, "rb");
  if (!src_file) {
    fprintf(stderr, "Error: Cannot open source executable '%s'\n", executable_path);
    unlink(temp_bytecode_file);
    return 1;
  }

  FILE* dst_file = fopen(output_name, "wb");
  if (!dst_file) {
    fprintf(stderr, "Error: Cannot create target executable '%s'\n", output_name);
    fclose(src_file);
    unlink(temp_bytecode_file);
    return 1;
  }

  // Copy file contents
  char copy_buffer[8192];
  size_t bytes;
  while ((bytes = fread(copy_buffer, 1, sizeof(copy_buffer), src_file)) > 0) {
    if (fwrite(copy_buffer, 1, bytes, dst_file) != bytes) {
      fprintf(stderr, "Error: Failed to write to target executable\n");
      fclose(src_file);
      fclose(dst_file);
      unlink(temp_bytecode_file);
      unlink(output_name);
      return 1;
    }
  }

  fclose(src_file);
  fclose(dst_file);

  // Step 5: Append bytecode to the executable with a signature
  FILE* output_file = fopen(output_name, "ab");  // Append mode
  FILE* bytecode_file = fopen(temp_bytecode_file, "rb");

  if (!output_file || !bytecode_file) {
    fprintf(stderr, "Error: Failed to open files for bytecode embedding\n");
    if (output_file) fclose(output_file);
    if (bytecode_file) fclose(bytecode_file);
    unlink(temp_bytecode_file);
    unlink(output_name);
    return 1;
  }

  // Get file size for the binary bytecode file
  fseek(bytecode_file, 0, SEEK_END);
  long file_bytecode_size = ftell(bytecode_file);
  fseek(bytecode_file, 0, SEEK_SET);

  // Verify size matches what we wrote
  if ((size_t)file_bytecode_size != bytecode_size) {
    fprintf(stderr, "Error: Bytecode file size mismatch\n");
    fclose(output_file);
    fclose(bytecode_file);
    unlink(temp_bytecode_file);
    unlink(output_name);
    return 1;
  }

  // Write bytecode
  char buffer[4096];
  size_t bytecode_bytes;
  while ((bytecode_bytes = fread(buffer, 1, sizeof(buffer), bytecode_file)) > 0) {
    fwrite(buffer, 1, bytecode_bytes, output_file);
  }

  // Write boundary and bytecode size as 8-byte big-endian for runtime detection
  const char* boundary = "JSRT_BYTECODE_BOUNDARY";
  fwrite(boundary, 1, strlen(boundary), output_file);

  // Write bytecode size as 8-byte big-endian
  uint64_t size_be = 0;
  for (int i = 0; i < 8; i++) {
    size_be = (size_be << 8) | ((file_bytecode_size >> (8 * (7 - i))) & 0xFF);
  }
  fwrite(&size_be, 1, 8, output_file);

  fclose(output_file);
  fclose(bytecode_file);
  unlink(temp_bytecode_file);

  // Make executable using chmod system call
  if (chmod(output_name, 0755) != 0) {
    fprintf(stderr, "Warning: Failed to set executable permissions on %s\n", output_name);
  }

  printf("✓ Build completed successfully: %s\n", output_name);
  printf("  Type: Self-contained executable with embedded bytecode\n");
  printf("  Size: Original + %ld bytes bytecode\n", file_bytecode_size);
  printf("  Usage: ./%s [args]\n", output_name);

  return 0;
}