#!/bin/sh

# Generates a png hex and associated parameters used to build the splash screen.
# Not part of the build process - should only be used after modifications to the splash screen

printf "Splash screen export"

MISSING_DEP=false;

depCheck() {
    if ! type "$1" >/dev/null 2>&1; then
	printf "\nCould not find $1!\n"
	MISSING_DEP=true;
    fi
};

# Requires inkscape (pref. v 0.91 or higher), hexdump and sed

depCheck "inkscape";
depCheck "hexdump";
depCheck "sed";

if [ "$MISSING_DEP" = true ]; then
    printf "\nOne or more dependencies missing!"
    exit 1;
fi

FN="splash_screen"

# Extract width and height values from the svg - somehwat reliant on inkscapes svg formatting
WIDTH=$(sed -n -E '1,/\s*width=/ {/width/ s/.*width="(.*)"/\1/p;}' "$FN".svg)
HEIGHT=$(sed -n -E '1,/\s*height=/ {/height/ s/.*height="(.*)"/\1/p;}' "$FN".svg)

printf " - Width x Height = ""$WIDTH""x""$HEIGHT\n"

# export png
inkscape --export-png="$FN".png --export-area-page "$FN".svg > /dev/null &&\

printf "png exported\n" &&\

# hex array generation
hexdump -ve '1 1 "0x%02x,"' "$FN".png > SplashPngHex && \

printf "hex data updated\n" &&\

#update width/height values
sed -i -E \
       -e 's/(Width\s*=\s*)([0-9]*)/\1'"$WIDTH"'/' \
       -e 's/(Height\s*=\s*)([0-9]*)/\1'"$HEIGHT"'/' \
       -e 's/(Length\s*=\s*)([0-9]*)/\1'"$(du -b $FN.png | cut -f 1)"'/' ../Misc/Splash.cpp &&\

printf "parameter values updated\n"

# unconditional cleanup
rm -f "$FN".png && printf "cleaning up\n"
