#!/bin/sh

# Generates a hex for the Yoshimi png icon found in ../desktop/128/yoshimi.png
# Not part of the build process - should only be used after modifications to the yoshimi icon
# Input file is ../desktop/128/yoshimi.png
# Adapted and very much simplified from the similar script to generate the splash

depCheck()
{
    if ! type "$1" >/dev/null 2>&1; then
        echo Could not find $1!
        MISSING_DEP=true;
    fi
}

echo
echo "Yoshimi Icon export..."

PNG_FILE="../desktop/128/yoshimi.png"
DEST=${0%/*}"/.."
HEX=$DEST"/src/UI/YoshimiIconPngHex"
depCheck "hexdump"
depCheck "sed"
if [ "$MISSING_DEP" = true ]; then
    echo One or more dependencies missing!
    echo
    exit 1
fi

# hex array generation just the raw image data
hexdump -ve '1 1 "0x%02x,"' $PNG_FILE | sed 's/,$//' > $HEX

echo hex icon updated to $HEX
