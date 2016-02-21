#!/bin/bash
# Linux run script for Sauerbraten Enhanced

# Variables

# what's the dir that contains Sauerbraten Enhanced data
SE_DIR="SauerEnhanced"

# default run options
#SE_OPTS="-t -r -k${SE_DIR} -q${SE_DIR}"
SE_OPTS="-r -k${SE_DIR} -q${SE_DIR}"

# Change keyboard layout before running if enabled
#SE_KEYB="us"

# architecture detection
SE_ARCH="$(uname -m)"
case $SE_ARCH in
    i486|i586|i686) unset SE_ARCH ;;
    x86_64) SE_ARCH="64" ;;
esac

# OS detection
SE_OS="$(uname -s)"
case $SE_OS in
    Linux) SE_OS="linux" ;;
    *) SE_OS="unknown" ;;
esac

# test if user has custom-compiled client
if [ -f "./${SE_DIR}/bin_unix/native_client" ]; then
    SE_BIN="./${SE_DIR}/bin_unix/native_client"
else
    [ -z "$SE_ARCH" ] && SE_BIN="./${SE_DIR}/bin_unix/se_linux" || SE_BIN="./${SE_DIR}/bin_unix/se_linux_64"
fi

# If user wants to set keyboard layout before running, set it
if [ "$SE_KEYB" ]; then
    SE_SAVED_KEYB="$(setxkbmap -print | grep xkb_symbols | egrep -o '\+.*\+' | sed -e 's/+//g'|cut -c 1-2)"
    setxkbmap ${SE_KEYB}
fi

# Run Sauer Enhanced
if [ -x "$SE_BIN" ]; then
    exec ${SE_BIN} ${SE_OPTS} "$@"
else
    echo "No client found. You can get a binary if available, or compile client yourself from included source."
    exit 1
fi

# set keyboard back if set after playing
if [ "$SE_KEYB" ]; then
    setxkbmap ${SE_SAVED_KEYB}
fi

exit 0
