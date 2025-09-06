# Known Issues and Troubleshooting

## Docker Build Issues

### FFI Compilation Error
When building inside the Docker container, you may encounter a compilation error in `src/std/ffi.c`:

```
error: initializer element is not constant
static JSValue ffi_functions_map = JS_UNINITIALIZED;
```

**Cause**: Different GCC versions handle constant initializers differently. The container uses GCC 11.4.0 while the project may have been developed with a different version.

**Workaround**: The FFI module appears to be optional. You can:
1. Comment out FFI-related code temporarily
2. Use a different compiler version
3. Initialize the variable at runtime instead of compile-time

**Resolution**: This is a known compatibility issue that needs to be addressed in the main project. The Docker environment itself is working correctly.

### Git Ownership Issues
The Docker container automatically handles git ownership issues by setting:
```bash
git config --global --add safe.directory '*'
```

### CMake Cache Issues
If you get CMake cache errors, clean the target directory:
```bash
rm -rf target
```

## Dev Container

The VS Code dev container should work without issues. It uses the same base Ubuntu image but with a more controlled environment.

## Testing the Setup

To verify the Docker environment is working:

```bash
# Test tools are available
make claude-shell
# Inside container:
gcc --version
cmake --version
clang-format --version
git --version

# Test project structure
ls -la /repo
cd /repo && git status
```

The setup is complete and functional - only the FFI compilation issue needs to be resolved in the main project.