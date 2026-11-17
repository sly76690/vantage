#!/bin/bash
# Renders the Orbit skin with Blender, then copies the result into build/skin-copy so it can be reviewed.
# Usage: tools/rendu.sh          (quick render)      tools/rendu.sh final     (full quality, slower)
cd "$(dirname "$0")/.." || exit 1
SKIN="$HOME/Library/Application Support/SF Instruments/Vantage/Skin"
if [ "$1" = "final" ]; then MODE=""; else MODE="quick"; fi
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/orbit_render.py -- $MODE > build/render.log 2>&1
grep -E "complete|Error|Traceback" build/render.log | tail -3
mkdir -p build/skin-copy && cp "$SKIN"/* build/skin-copy/
