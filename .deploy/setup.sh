#!/usr/bin/env bash

SOURCE_PATH="../../../../zeal8bit/Zeal-NativeEmulator/build-wasm-minimal/"
DEST_PATH="."
FILES=("zeal.elf.js" "zeal.elf.wasm")
FULL_FILES=("index.css" "index.html" "index.js" "zeal.elf.js" "zeal.elf.wasm")

# Loop through each file and copy it
for file in "${FILES[@]}"; do
    echo "Copying $file..."
    cp "$SOURCE_PATH$file" "$DEST_PATH/$file"
done

echo "All files copied successfully!"