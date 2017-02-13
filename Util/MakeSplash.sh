#!/bin/sh

# Generates a png hex and associated parameters used to build the splash screen.
# Not part of the build process - should only be used after modifications to the splash screen
# Input files can be .svg or .png

depCheck()
{
    if ! type "$1" >/dev/null 2>&1; then
        echo Could not find $1!
        MISSING_DEP=true;
    fi
}
echo
echo Splash screen export

if [ -z $1 ] || [ ! -r $1 ]
then
    echo Needs a valid filename!
    echo
    exit 2
fi

DEST=${0%/*}"/.."
FILE=$1
HEX=$DEST"/src/Misc/SplashPngHex"
CODE=$DEST"/src/Misc/Splash.cpp"
EXTEN="${FILE##*.}"

MISSING_DEP=false

if [ "$EXTEN" = "svg" ]; then
    # Requires inkscape (pref. v 0.91 or higher), hexdump and sed
    depCheck "inkscape"
    depCheck "hexdump"
    depCheck "sed"

    if [ "$MISSING_DEP" = true ]; then
        echo One or more dependencies missing!
        echo
        exit 1
    fi
    # Extract width and height values from the svg - somewhat reliant on inkscapes svg formatting
    WIDTH=$(sed -n -E '1,/\s*width=/ {/width/ s/.*width="(.*)"/\1/p;}' "$FILE")
    HEIGHT=$(sed -n -E '1,/\s*height=/ {/height/ s/.*height="(.*)"/\1/p;}' "$FILE")

    echo svg - Width x Height = "$WIDTH"x"$HEIGHT"
    echo $PROG

    FN="/tmp/splash_screen.png"

    # export png
    inkscape --export-png="$FN" --export-area-page "$FILE" > /dev/null &&\

    echo exported png
elif [ "$EXTEN" = "png" ]; then
    depCheck "hexdump"
    depCheck "sed"

    if [ "$MISSING_DEP" = true ]; then
        echo One or more dependencies missing!
        echo
        exit 1
    fi
    # Extract width and height values
    SIZE=$(file $FILE | cut -d ',' -f 2 | sed 's/ //g')
    WIDTH=$(echo $SIZE | cut -d 'x' -f 1)
    HEIGHT=$(echo $SIZE | cut -d 'x' -f 2)
    echo png - Width x Height = "$WIDTH"x"$HEIGHT"
    FN=$FILE;
else
    echo Invalid file type
fi

# hex array generation
hexdump -ve '1 1 "0x%02x,"' "$FN" > $HEX && \

echo hex data updated

#update width/height values
sed -i -E \
       -e 's/(Width\s*=\s*)([0-9]*)/\1'"$WIDTH"'/' \
       -e 's/(Height\s*=\s*)([0-9]*)/\1'"$HEIGHT"'/' \
       -e 's/(Length\s*=\s*)([0-9]*)/\1'"$(du -b $FN | cut -f 1)"'/' $CODE &&\

echo "parameter values updated"

# cleanup
if [ "$EXTEN" = "svg" ]; then
    rm -f "$FN"
    echo cleaning up
    echo
else
    echo
fi
