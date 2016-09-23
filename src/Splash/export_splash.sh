#!/bin/bash

# Convert splash_screen.svg to a static header with a hex of the rendered png
# Not part of the build process - should only be used after modifications to the splash screen

printf "Splash screen export\n"

MISSING_DEP=false;

function depCheck {
    if ! type "$1" >/dev/null 2>&1; then
	printf "Could not find $1!\n"
	MISSING_DEP=true;
    fi
};

# Requires inkscape (pref. v 0.91 or higher), xxd and sed

depCheck "inkscape";
depCheck "xxd";
depCheck "sed";

if [[ "$MISSING_DEP" = true ]]; then
    printf "One or more dependencies missing!"
    exit 1;
fi

FN="splash_screen"
TMP="$FN"_tmp

# Extract width and height values from the svg - reliant on the xml formatting
WIDTH=$(sed -n -E '1,/\w*width=/ {/width/ s/.*width="(.*)"/\1/p;}' "$FN".svg)
HEIGHT=$(sed -n -E '1,/\w*height=/ {/height/ s/.*height="(.*)"/\1/p;}' "$FN".svg)

# export png
inkscape --export-png="$FN".png --export-area-page "$FN".svg > /dev/null &&\

printf "png exported\n" &&\

# probably redundant declaration guard
printf \
"#ifdef __cplusplus
extern \"C\" {
#endif

" > "$TMP" && \

# splash dimension variables
printf \
"static const int splashWidth=$WIDTH;
static const int splashHeight=$HEIGHT;

static const " >> "$TMP" && \

# hex array generation
xxd -i "$FN".png >>  "$TMP" && \

# closing declaration guard
printf \
"
#ifdef __cplusplus
}
#endif
" >> "$TMP" && \

mv "$TMP" "$FN".h &&\

printf "header created\n"

# unconditional cleanup
rm -f "$TMP" "$FN".png && printf "cleaning up temp files\n"
