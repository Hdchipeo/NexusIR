#!/bin/bash
# compress_web.sh - Automates compressing frontend assets from web_src/ to www/

BASE_DIR="$(dirname "$0")"
SRC_DIR="$BASE_DIR/web_src"
WWW_DIR="$BASE_DIR/www"

if [ ! -d "$SRC_DIR" ]; then
    echo "Error: Source directory $SRC_DIR not found."
    exit 1
fi

mkdir -p "$WWW_DIR"
# Clean up old files in www
rm -f "$WWW_DIR"/*

echo "Compressing web assets from $SRC_DIR to $WWW_DIR..."

# Compress files using gzip -c to output directly to the destination folder
gzip -c "$SRC_DIR/index.html" > "$WWW_DIR/index.html.gz"
gzip -c "$SRC_DIR/app.js" > "$WWW_DIR/app.js.gz"
gzip -c "$SRC_DIR/style.css" > "$WWW_DIR/style.css.gz"

echo "Compression complete!"
ls -lh "$WWW_DIR"
