#!/bin/bash

# Script to remove license headers from UI image and font files
# These are auto-generated files and should not have license headers

echo "Removing license headers from UI image files..."

find app/src/ui/images -type f -name "*.c" | while read -r file; do
    if grep -q "SPDX-License-Identifier" "$file"; then
        echo "Removing header from: $file"
        # Remove the first 24 lines (license header) and blank line
        tail -n +26 "$file" > "$file.tmp"
        mv "$file.tmp" "$file"
    fi
done

echo "Removing license headers from UI font files..."

find app/src/ui/fonts -type f -name "*.c" | while read -r file; do
    if grep -q "SPDX-License-Identifier" "$file"; then
        echo "Removing header from: $file"
        # Remove the first 24 lines (license header) and blank line
        tail -n +26 "$file" > "$file.tmp"
        mv "$file.tmp" "$file"
    fi
done

echo "License header removal from image and font files complete!"
