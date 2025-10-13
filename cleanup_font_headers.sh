#!/bin/bash

# Clean up any remaining license header fragments from font files

echo "Cleaning up any remaining license fragments from font files..."

find app/src/ui/fonts -type f -name "*.c" | while read -r file; do
    # Check if file starts with partial license text
    if head -n 1 "$file" | grep -q "OUT OF OR IN CONNECTION"; then
        echo "Cleaning up: $file"
        # Remove lines until we find a line that doesn't start with * or is empty after */
        # Skip first 3 lines (the fragment), plus any blank lines
        tail -n +4 "$file" | sed '/^$/d;q' > /dev/null
        tail -n +5 "$file" > "$file.tmp"
        mv "$file.tmp" "$file"
    fi
done

echo "Font file cleanup complete!"
