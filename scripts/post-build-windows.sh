#!/bin/bash
# Post-build script to restore MSYS2 environment
if [[ -n "$ORIGINAL_MSYSTEM" ]]; then
    export MSYSTEM="$ORIGINAL_MSYSTEM"
    echo "Restored MSYSTEM: $MSYSTEM"
fi
