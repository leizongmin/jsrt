#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jsrt.h"
#include "util/file.h"

void PrintHelp(bool is_error);
void PrintVersion(const char* executable_path);
int BuildExecutable(const char* filename, const char* target);

int main(int argc, char** argv) {
  // Handle explicit stdin flag first
  if (argc == 2 && strcmp(argv[1], "-") == 0) {
    int ret = JSRT_CmdRunStdin(argc, argv);
    return ret;
  }
  if (argc < 2) {
    // Check if this executable contains embedded bytecode when no args provided
    // This handles the case when the executable is self-contained
    int ret = JSRT_CmdRunEmbeddedBytecode(argv[0], argc, argv);
    if (ret == 0) {
      return ret;  // Successfully executed embedded bytecode
    }

    // If no embedded bytecode, check for piped stdin input
    if (!isatty(STDIN_FILENO)) {
      ret = JSRT_CmdRunStdin(argc, argv);
      return ret;
    }

    PrintHelp(true);
    return 1;
  }

  const char* command = argv[1];

  // Handle special commands
  if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
    PrintHelp(false);
    return 0;
  }

  if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
    PrintVersion(argv[0]);
    return 0;
  }

  if (strcmp(command, "build") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: build command requires a filename\n");
      fprintf(stderr, "Usage: jsrt build <filename> [target]\n");
      return 1;
    }
    const char* filename = argv[2];
    const char* target = argc >= 4 ? argv[3] : NULL;
    return BuildExecutable(filename, target);
  }

  // If no embedded bytecode, handle regular file execution
  int ret = JSRT_CmdRunFile(command, argc, argv);

  return ret;
}

void PrintHelp(bool is_error) {
  fprintf(is_error ? stderr : stdout,
          "Welcome to jsrt, a small JavaScript runtime.\n"
          "Author:   LEI Zongmin <leizongmin@gmail.com>\n"
          "Homepage: https://github.com/leizongmin/jsrt\n"
          "License:  MIT\n"
          "\n"
          "Usage: jsrt <filename> [args]            Run script file\n"
          "       jsrt <url> [args]                 Run script from URL\n"
          "       jsrt build <filename> [target]    Create self-contained binary file\n"
          "       jsrt repl                         Run REPL\n"
          "       jsrt version                      Print version\n"
          "       jsrt help                         Print this help message\n"
          "       jsrt -                            Read JavaScript code from stdin\n"
          "       echo 'code' | jsrt                Pipe JavaScript code from stdin\n"
          "\n");
}

void PrintVersion(const char* executable_path) {
#ifdef JSRT_VERSION
  const char* version = JSRT_VERSION;
#else
  const char* version = "unknown";
#endif

  printf("jsrt v%s\n", version);
  printf("A lightweight, fast JavaScript runtime built on QuickJS and libuv\n");
  printf("Copyright © 2024-2025 LEI Zongmin\n");
  printf("License: MIT\n");
}

int BuildExecutable(const char* filename, const char* target) {
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

  // Check if input file exists
  JSRT_ReadFileResult input_file = JSRT_ReadFile(filename);
  if (input_file.error != JSRT_READ_FILE_OK) {
    fprintf(stderr, "Error: Cannot read input file '%s': %s\n", filename, JSRT_ReadFileErrorToString(input_file.error));
    return 1;
  }
  JSRT_ReadFileResultFree(&input_file);

  // Step 1: Find qjsc binary
  char qjsc_path[1024] = {0};

  if (access("./qjsc", X_OK) == 0) {
    strcpy(qjsc_path, "./qjsc");
  } else if (access("../qjsc", X_OK) == 0) {
    strcpy(qjsc_path, "../qjsc");
  } else if (access("target/release/qjsc", X_OK) == 0) {
    strcpy(qjsc_path, "target/release/qjsc");
  } else {
    fprintf(stderr, "Error: qjsc compiler not found. Please build with BUILD_QJS=ON.\n");
    return 1;
  }

  printf("Using qjsc: %s\n", qjsc_path);

  // Step 2: Compile JavaScript to bytecode using qjsc
  char temp_bytecode_file[256];
  snprintf(temp_bytecode_file, sizeof(temp_bytecode_file), "/tmp/jsrt_build_%d.bin", (int)getpid());

  char qjsc_command[2048];
  char temp_c_file[256];
  snprintf(temp_c_file, sizeof(temp_c_file), "/tmp/jsrt_build_%d.c", (int)getpid());
  snprintf(qjsc_command, sizeof(qjsc_command), "%s -c -o %s %s", qjsc_path, temp_c_file, filename);

  printf("Compiling JavaScript to bytecode...\n");
  int qjsc_result = system(qjsc_command);
  if (qjsc_result != 0) {
    fprintf(stderr, "Error: qjsc compilation failed\n");
    unlink(temp_c_file);
    return 1;
  }

  // Extract bytecode from the generated C file
  JSRT_ReadFileResult c_file = JSRT_ReadFile(temp_c_file);
  if (c_file.error != JSRT_READ_FILE_OK) {
    fprintf(stderr, "Error: Cannot read generated C file\n");
    unlink(temp_c_file);
    return 1;
  }

  // Parse the bytecode array from C file and write binary file
  char* array_start = strstr(c_file.data, "= {");
  char* size_line = strstr(c_file.data, "const uint32_t qjsc_");
  size_t bytecode_size = 0;

  if (size_line) {
    char* size_start = strstr(size_line, " = ");
    if (size_start) {
      sscanf(size_start + 3, "%zu", &bytecode_size);
    }
  }

  if (!array_start || bytecode_size == 0) {
    fprintf(stderr, "Error: Cannot parse bytecode from C file\n");
    JSRT_ReadFileResultFree(&c_file);
    unlink(temp_c_file);
    return 1;
  }

  // Create binary bytecode file
  FILE* bytecode_file_fp = fopen(temp_bytecode_file, "wb");
  if (!bytecode_file_fp) {
    fprintf(stderr, "Error: Cannot create bytecode file\n");
    JSRT_ReadFileResultFree(&c_file);
    unlink(temp_c_file);
    return 1;
  }

  // Parse and write hex values
  array_start += 3;  // Skip "= {"
  char* array_end = strstr(array_start, "};");
  char* pos = array_start;
  size_t bytes_written = 0;

  while (pos < array_end && bytes_written < bytecode_size) {
    while (*pos && (*pos == ' ' || *pos == '\n' || *pos == '\t' || *pos == ',')) pos++;
    if (*pos == '0' && *(pos + 1) == 'x') {
      unsigned int hex_val;
      if (sscanf(pos, "0x%02x", &hex_val) == 1) {
        unsigned char byte_val = (unsigned char)hex_val;
        fwrite(&byte_val, 1, 1, bytecode_file_fp);
        bytes_written++;
        pos += 4;  // Skip "0xXX"
      } else {
        pos++;
      }
    } else {
      pos++;
    }
  }

  fclose(bytecode_file_fp);
  JSRT_ReadFileResultFree(&c_file);
  unlink(temp_c_file);

  if (bytes_written != bytecode_size) {
    fprintf(stderr, "Error: Bytecode size mismatch (expected %zu, got %zu)\n", bytecode_size, bytes_written);
    unlink(temp_bytecode_file);
    return 1;
  }

  // Step 3: Copy current jsrt executable as base
  char cp_command[1024];
  snprintf(cp_command, sizeof(cp_command), "cp ./bin/jsrt %s", output_name);
  printf("Creating self-contained executable...\n");

  int cp_result = system(cp_command);
  if (cp_result != 0) {
    fprintf(stderr, "Error: Failed to copy jsrt executable\n");
    unlink(temp_bytecode_file);
    return 1;
  }

  // Step 4: Append bytecode to the executable with a signature
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

  // Write bytecode
  char buffer[4096];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer), bytecode_file)) > 0) {
    fwrite(buffer, 1, bytes, output_file);
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

  // Make executable
  char chmod_command[512];
  snprintf(chmod_command, sizeof(chmod_command), "chmod +x %s", output_name);
  system(chmod_command);

  printf("✓ Build completed successfully: %s\n", output_name);
  printf("  Type: Self-contained executable with embedded bytecode\n");
  printf("  Size: Original + %ld bytes bytecode\n", file_bytecode_size);
  printf("  Usage: ./%s [args]\n", output_name);

  return 0;
}
