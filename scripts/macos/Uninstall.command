#!/bin/bash
set -e
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
APP_DIR="$HOME/Applications"

echo "Uninstalling PF-02..."
rm -rf "$AU_DIR/PF-02.component"
rm -rf "$VST3_DIR/PF-02.vst3"
rm -rf "$CLAP_DIR/PF-02.clap"
rm -rf "$APP_DIR/PF-02.app"

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "PF-02 has been removed."
read -n 1 -s -r -p "Press any key to close this window..."
echo ""
