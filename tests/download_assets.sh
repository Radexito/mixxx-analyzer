#!/usr/bin/env bash
# Downloads royalty-free test audio files from Audionautix (CC BY 4.0).
# Run from the repository root:  bash tests/download_assets.sh
# Jason Shaw / Audionautix — https://audionautix.com

set -euo pipefail

ASSETS_DIR="$(dirname "$0")/assets"
mkdir -p "$ASSETS_DIR"

URLS=(
    "https://audionautix.com/Music/FallingSky.mp3"
    "https://audionautix.com/Music/LatinHouseBed.mp3"
    "https://audionautix.com/Music/BanjoHop.mp3"
    "https://audionautix.com/Music/BeBop25.mp3"
    "https://audionautix.com/Music/Boom.mp3"
    "https://audionautix.com/Music/NightRave.mp3"
    "https://audionautix.com/Music/Sk8board.mp3"
    "https://audionautix.com/Music/AllGoodInTheWood.mp3"
    "https://audionautix.com/Music/DanceDubber.mp3"
    "https://audionautix.com/Music/DogHouse.mp3"
    "https://audionautix.com/Music/Don'tStop.mp3"
)

for url in "${URLS[@]}"; do
    filename=$(basename "$url")
    dest="$ASSETS_DIR/$filename"
    if [[ -f "$dest" ]]; then
        echo "  already present: $filename"
    else
        echo "  downloading: $filename"
        curl -fsSL --retry 3 -o "$dest" "$url"
    fi
done

echo "Done. Files in $ASSETS_DIR:"
ls -lh "$ASSETS_DIR"
