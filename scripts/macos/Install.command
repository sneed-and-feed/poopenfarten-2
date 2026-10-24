#!/bin/bash
# ==============================================================================
# POOPENFARTEN II (PF-02) — Easy Installer for macOS (Universal: ARM64 & Intel)
# Double-click this script in Finder to install AU, VST3, CLAP, and Standalone.
# ==============================================================================

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
ORANGE='\033[0;33m'
RESET='\033[0m'

echo ""
echo -e "${BOLD}${ORANGE}================================================================${RESET}"
echo -e "${BOLD}${ORANGE}   PF-02 TRIADEN-SYNTHESIZER — macOS UNIVERSAL EASY INSTALLER   ${RESET}"
echo -e "${BOLD}${ORANGE}================================================================${RESET}"
echo ""

ARCH=$(uname -m)
echo -e "${CYAN}Target System Architecture:${RESET} ${BOLD}${ARCH}${RESET}"
echo ""

AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
APP_DIR="$HOME/Applications"

echo "Creating plugin destination directories..."
mkdir -p "$AU_DIR" "$VST3_DIR" "$CLAP_DIR" "$APP_DIR"

# 1. Install Audio Unit (.component)
AU_SRC=$(find "$DIR" -maxdepth 2 -name "PF-02.component" -type d | head -n 1 || true)
if [ -n "$AU_SRC" ] && [ -d "$AU_SRC" ]; then
    echo -e "Installing ${BOLD}Audio Unit (AU)${RESET} -> $AU_DIR/PF-02.component..."
    rm -rf "$AU_DIR/PF-02.component"
    cp -R "$AU_SRC" "$AU_DIR/"
    xattr -cr "$AU_DIR/PF-02.component" 2>/dev/null || true
    codesign --force --deep --sign - "$AU_DIR/PF-02.component" 2>/dev/null || true
    echo -e "${GREEN}  ✓ AU installed, signed, and unquarantined${RESET}"
fi

# 2. Install VST3 (.vst3)
VST3_SRC=$(find "$DIR" -maxdepth 2 -name "PF-02.vst3" -type d | head -n 1 || true)
if [ -n "$VST3_SRC" ] && [ -d "$VST3_SRC" ]; then
    echo -e "Installing ${BOLD}VST3${RESET} -> $VST3_DIR/PF-02.vst3..."
    rm -rf "$VST3_DIR/PF-02.vst3"
    cp -R "$VST3_SRC" "$VST3_DIR/"
    xattr -cr "$VST3_DIR/PF-02.vst3" 2>/dev/null || true
    codesign --force --deep --sign - "$VST3_DIR/PF-02.vst3" 2>/dev/null || true
    echo -e "${GREEN}  ✓ VST3 installed, signed, and unquarantined${RESET}"
fi

# 3. Install CLAP (.clap)
CLAP_SRC=$(find "$DIR" -maxdepth 2 -name "PF-02.clap" | head -n 1 || true)
if [ -n "$CLAP_SRC" ] && [ -e "$CLAP_SRC" ]; then
    echo -e "Installing ${BOLD}CLAP${RESET} -> $CLAP_DIR/PF-02.clap..."
    rm -rf "$CLAP_DIR/PF-02.clap"
    cp -R "$CLAP_SRC" "$CLAP_DIR/"
    xattr -cr "$CLAP_DIR/PF-02.clap" 2>/dev/null || true
    codesign --force --deep --sign - "$CLAP_DIR/PF-02.clap" 2>/dev/null || true
    echo -e "${GREEN}  ✓ CLAP installed, signed, and unquarantined${RESET}"
fi

# 4. Install Standalone (.app)
APP_SRC=$(find "$DIR" -maxdepth 2 -name "PF-02.app" -type d | head -n 1 || true)
if [ -n "$APP_SRC" ] && [ -d "$APP_SRC" ]; then
    echo -e "Installing ${BOLD}Standalone App${RESET} -> $APP_DIR/PF-02.app..."
    rm -rf "$APP_DIR/PF-02.app"
    cp -R "$APP_SRC" "$APP_DIR/"
    xattr -cr "$APP_DIR/PF-02.app" 2>/dev/null || true
    codesign --force --deep --sign - "$APP_DIR/PF-02.app" 2>/dev/null || true
    echo -e "${GREEN}  ✓ Standalone app installed, signed, and unquarantined${RESET}"
fi

echo ""
echo "Flushing macOS CoreAudio plugin cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo -e "${BOLD}${GREEN}================================================================${RESET}"
echo -e "${BOLD}${GREEN}           INSTALLATION COMPLETE — PF-02 IS READY!              ${RESET}"
echo -e "${BOLD}${GREEN}================================================================${RESET}"
echo ""
read -n 1 -s -r -p "Press any key to close this window..."
echo ""
