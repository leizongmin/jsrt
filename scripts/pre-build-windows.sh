#!/bin/bash
# Pre-build script for Windows to fix libuv platform detection issues

echo "Pre-build Windows fixes..."

# Check if we're in MSYS2 environment
if [[ -n "$MSYSTEM" ]]; then
    echo "MSYS2 environment detected: $MSYSTEM"
    
    # Temporarily clear MSYS2 environment variables that confuse libuv
    export ORIGINAL_MSYSTEM="$MSYSTEM"
    unset MSYSTEM
    unset CYGWIN
    
    echo "Temporarily cleared MSYSTEM and CYGWIN environment variables"
    
    # Create a wrapper script to restore environment after build
    cat > scripts/post-build-windows.sh << 'EOF'
#!/bin/bash
# Post-build script to restore MSYS2 environment
if [[ -n "$ORIGINAL_MSYSTEM" ]]; then
    export MSYSTEM="$ORIGINAL_MSYSTEM"
    echo "Restored MSYSTEM: $MSYSTEM"
fi
EOF
    chmod +x scripts/post-build-windows.sh
fi

# Apply aggressive libuv Windows-only patch
echo "Applying aggressive libuv Windows patch..."
chmod +x scripts/patch-libuv-windows.sh
./scripts/patch-libuv-windows.sh

echo "Pre-build Windows fixes completed"