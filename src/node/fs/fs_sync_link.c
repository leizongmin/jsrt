#include "fs_common.h"
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>
#else
#include <sys/stat.h>
#endif

// fs.linkSync(existingPath, newPath)
JSValue js_fs_link_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "existingPath and newPath are required");
    }

    const char* existing_path = JS_ToCString(ctx, argv[0]);
    if (!existing_path) {
        return JS_EXCEPTION;
    }

    const char* new_path = JS_ToCString(ctx, argv[1]);
    if (!new_path) {
        JS_FreeCString(ctx, existing_path);
        return JS_EXCEPTION;
    }

#ifdef _WIN32
    // Windows: Use CreateHardLink
    if (!CreateHardLinkA(new_path, existing_path, NULL)) {
        DWORD error = GetLastError();
        JSValue fs_error;
        
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                fs_error = create_fs_error(ctx, ENOENT, "link", existing_path);
                break;
            case ERROR_ACCESS_DENIED:
                fs_error = create_fs_error(ctx, EACCES, "link", new_path);
                break;
            case ERROR_ALREADY_EXISTS:
                fs_error = create_fs_error(ctx, EEXIST, "link", new_path);
                break;
            case ERROR_NOT_SAME_DEVICE:
                fs_error = create_fs_error(ctx, EXDEV, "link", new_path);
                break;
            default:
                fs_error = create_fs_error(ctx, EIO, "link", new_path);
                break;
        }
        
        JS_FreeCString(ctx, existing_path);
        JS_FreeCString(ctx, new_path);
        return JS_Throw(ctx, fs_error);
    }
#else
    // Unix/Linux: Use link()
    if (link(existing_path, new_path) < 0) {
        JSValue error = create_fs_error(ctx, errno, "link", 
                                      errno == ENOENT ? existing_path : new_path);
        JS_FreeCString(ctx, existing_path);
        JS_FreeCString(ctx, new_path);
        return JS_Throw(ctx, error);
    }
#endif

    JS_FreeCString(ctx, existing_path);
    JS_FreeCString(ctx, new_path);
    return JS_UNDEFINED;
}

// fs.symlinkSync(target, path[, type])
JSValue js_fs_symlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "target and path are required");
    }

    const char* target = JS_ToCString(ctx, argv[0]);
    if (!target) {
        return JS_EXCEPTION;
    }

    const char* path = JS_ToCString(ctx, argv[1]);
    if (!path) {
        JS_FreeCString(ctx, target);
        return JS_EXCEPTION;
    }

    // Type parameter (optional, mainly for Windows)
    const char* type = "file";  // default
    if (argc >= 3 && !JS_IsUndefined(argv[2])) {
        type = JS_ToCString(ctx, argv[2]);
        if (!type) {
            JS_FreeCString(ctx, target);
            JS_FreeCString(ctx, path);
            return JS_EXCEPTION;
        }
    }

#ifdef _WIN32
    // Windows: Use CreateSymbolicLink
    DWORD flags = 0;
    if (strcmp(type, "dir") == 0 || strcmp(type, "directory") == 0) {
        flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
    }
    // Enable unprivileged symlinks on Windows 10 Creators Update+
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

    if (!CreateSymbolicLinkA(path, target, flags)) {
        DWORD error = GetLastError();
        JSValue fs_error;
        
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                fs_error = create_fs_error(ctx, ENOENT, "symlink", path);
                break;
            case ERROR_ACCESS_DENIED:
            case ERROR_PRIVILEGE_NOT_HELD:
                fs_error = create_fs_error(ctx, EACCES, "symlink", path);
                break;
            case ERROR_ALREADY_EXISTS:
                fs_error = create_fs_error(ctx, EEXIST, "symlink", path);
                break;
            default:
                fs_error = create_fs_error(ctx, EIO, "symlink", path);
                break;
        }
        
        JS_FreeCString(ctx, target);
        JS_FreeCString(ctx, path);
        if (argc >= 3 && !JS_IsUndefined(argv[2])) {
            JS_FreeCString(ctx, type);
        }
        return JS_Throw(ctx, fs_error);
    }
#else
    // Unix/Linux: Use symlink()
    if (symlink(target, path) < 0) {
        JSValue error = create_fs_error(ctx, errno, "symlink", path);
        JS_FreeCString(ctx, target);
        JS_FreeCString(ctx, path);
        if (argc >= 3 && !JS_IsUndefined(argv[2])) {
            JS_FreeCString(ctx, type);
        }
        return JS_Throw(ctx, error);
    }
#endif

    JS_FreeCString(ctx, target);
    JS_FreeCString(ctx, path);
    if (argc >= 3 && !JS_IsUndefined(argv[2])) {
        JS_FreeCString(ctx, type);
    }
    return JS_UNDEFINED;
}

// fs.readlinkSync(path[, options])
JSValue js_fs_readlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "path is required");
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }

    // Parse options (encoding)
    const char* encoding = "utf8";  // default
    if (argc >= 2 && JS_IsObject(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue enc_val = JS_GetPropertyStr(ctx, argv[1], "encoding");
        if (!JS_IsUndefined(enc_val) && !JS_IsNull(enc_val)) {
            const char* enc_str = JS_ToCString(ctx, enc_val);
            if (enc_str) {
                encoding = enc_str;
            }
        }
        JS_FreeValue(ctx, enc_val);
    } else if (argc >= 2 && JS_IsString(argv[1])) {
        const char* enc_str = JS_ToCString(ctx, argv[1]);
        if (enc_str) {
            encoding = enc_str;
        }
    }

#ifdef _WIN32
    // Windows: Use GetFinalPathNameByHandle with symbolic link flag
    HANDLE handle = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        JSValue fs_error;
        
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                fs_error = create_fs_error(ctx, ENOENT, "readlink", path);
                break;
            case ERROR_ACCESS_DENIED:
                fs_error = create_fs_error(ctx, EACCES, "readlink", path);
                break;
            default:
                fs_error = create_fs_error(ctx, EIO, "readlink", path);
                break;
        }
        
        JS_FreeCString(ctx, path);
        return JS_Throw(ctx, fs_error);
    }

    char buffer[MAX_PATH];
    DWORD result = GetFinalPathNameByHandleA(handle, buffer, MAX_PATH, VOLUME_NAME_DOS);
    CloseHandle(handle);

    if (result == 0 || result >= MAX_PATH) {
        JSValue error = create_fs_error(ctx, EIO, "readlink", path);
        JS_FreeCString(ctx, path);
        return JS_Throw(ctx, error);
    }

    // Remove \\?\ prefix if present
    const char* link_target = buffer;
    if (strncmp(buffer, "\\\\?\\", 4) == 0) {
        link_target = buffer + 4;
    }
#else
    // Unix/Linux: Use readlink()
    char buffer[PATH_MAX];
    ssize_t len = readlink(path, buffer, sizeof(buffer) - 1);
    
    if (len < 0) {
        JSValue error = create_fs_error(ctx, errno, "readlink", path);
        JS_FreeCString(ctx, path);
        return JS_Throw(ctx, error);
    }
    
    buffer[len] = '\0';
    const char* link_target = buffer;
#endif

    JS_FreeCString(ctx, path);

    // Handle encoding
    if (strcmp(encoding, "buffer") == 0) {
        // Return as Buffer
        return create_buffer_from_data(ctx, (const uint8_t*)link_target, strlen(link_target));
    } else {
        // Return as string (utf8 default)
        return JS_NewString(ctx, link_target);
    }
}

// fs.realpathSync(path[, options])
JSValue js_fs_realpath_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "path is required");
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }

    // Parse options (encoding)
    const char* encoding = "utf8";  // default
    if (argc >= 2 && JS_IsObject(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue enc_val = JS_GetPropertyStr(ctx, argv[1], "encoding");
        if (!JS_IsUndefined(enc_val) && !JS_IsNull(enc_val)) {
            const char* enc_str = JS_ToCString(ctx, enc_val);
            if (enc_str) {
                encoding = enc_str;
            }
        }
        JS_FreeValue(ctx, enc_val);
    } else if (argc >= 2 && JS_IsString(argv[1])) {
        const char* enc_str = JS_ToCString(ctx, argv[1]);
        if (enc_str) {
            encoding = enc_str;
        }
    }

#ifdef _WIN32
    // Windows: Use GetFullPathName
    char buffer[MAX_PATH];
    DWORD result = GetFullPathNameA(path, MAX_PATH, buffer, NULL);
    
    if (result == 0 || result >= MAX_PATH) {
        JSValue error = create_fs_error(ctx, errno, "realpath", path);
        JS_FreeCString(ctx, path);
        return JS_Throw(ctx, error);
    }
    
    // Check if path exists
    if (GetFileAttributesA(buffer) == INVALID_FILE_ATTRIBUTES) {
        JSValue error = create_fs_error(ctx, ENOENT, "realpath", path);
        JS_FreeCString(ctx, path);
        return JS_Throw(ctx, error);
    }
    
    const char* real_path = buffer;
#else
    // Unix/Linux/macOS: Use realpath()
    char* resolved_path = realpath(path, NULL);
    
    if (!resolved_path) {
        JSValue error = create_fs_error(ctx, errno, "realpath", path);
        JS_FreeCString(ctx, path);
        return JS_Throw(ctx, error);
    }
    
    const char* real_path = resolved_path;
#endif

    JS_FreeCString(ctx, path);

    JSValue result_val;
    // Handle encoding
    if (strcmp(encoding, "buffer") == 0) {
        // Return as Buffer
        result_val = create_buffer_from_data(ctx, (const uint8_t*)real_path, strlen(real_path));
    } else {
        // Return as string (utf8 default)
        result_val = JS_NewString(ctx, real_path);
    }

#ifndef _WIN32
    free(resolved_path);
#endif

    return result_val;
}