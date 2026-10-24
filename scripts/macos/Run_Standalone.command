#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
APP_PATH="$DIR/PF-02.app"
if [ ! -d "$APP_PATH" ]; then
    APP_PATH="$DIR/Standalone/PF-02.app"
fi
if [ ! -d "$APP_PATH" ]; then
    APP_PATH="$HOME/Applications/PF-02.app"
fi
if [ -d "$APP_PATH" ]; then
    xattr -cr "$APP_PATH" 2>/dev/null || true
    codesign --force --deep --sign - "$APP_PATH" 2>/dev/null || true
    open "$APP_PATH"
else
    echo "PF-02.app not found. Please run Install.command first."
fi
