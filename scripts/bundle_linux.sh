#!/usr/bin/env bash
# bundle_linux.sh <binary> <out_dir>
#
# Copies <binary> and all non-system .so dependencies (transitively) into
# <out_dir>, then patches every file's RPATH to $ORIGIN so each binary/lib
# finds its peers without needing LD_LIBRARY_PATH.
#
# Requirements: patchelf (apt install patchelf)
set -euo pipefail

BINARY=$(realpath "$1")
OUT_DIR=$(realpath "$2")
mkdir -p "$OUT_DIR"

# Shared libs that must come from the OS — never bundle these.
SYSTEM_RE="(linux-vdso|ld-linux|libc\.so|libdl\.so|libpthread|librt\.so|libm\.so)"

VISITED=$(mktemp)
trap 'rm -f "$VISITED"' EXIT

get_deps() {
    ldd "$1" 2>/dev/null \
        | grep "=> /" \
        | awk '{print $3}' \
        | grep -vE "$SYSTEM_RE" \
        || true
}

process() {
    local lib="$1"
    local name
    name=$(basename "$lib")

    grep -qxF "$name" "$VISITED" && return
    echo "$name" >>"$VISITED"

    if [[ "$lib" != "$BINARY" ]] && [[ ! -f "$OUT_DIR/$name" ]]; then
        cp -L "$lib" "$OUT_DIR/$name"
        patchelf --set-rpath '$ORIGIN' "$OUT_DIR/$name" 2>/dev/null || true
    fi

    while IFS= read -r dep; do
        process "$dep"
    done < <(get_deps "$lib")
}

process "$BINARY"

# Copy the main binary last and patch its RPATH.
BINARY_NAME=$(basename "$BINARY")
cp "$BINARY" "$OUT_DIR/$BINARY_NAME"
chmod 755 "$OUT_DIR/$BINARY_NAME"
patchelf --set-rpath '$ORIGIN' "$OUT_DIR/$BINARY_NAME"

LIB_COUNT=$(find "$OUT_DIR" -maxdepth 1 -name '*.so*' | wc -l)
echo "Bundled ${LIB_COUNT} shared libs + binary into $OUT_DIR/"
