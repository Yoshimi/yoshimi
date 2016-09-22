#!/bin/bash

# Converts splash_screen.svg to a static header with a hex of the rendered png
# Not part of the build process - should only be used as part of a splash screen update

# Requires inkscape (pref. v 0.91 or higher) and xxd

FN=splash_screen
TMP="$FN"_tmp

#export png
inkscape --export-png=$FN.png --export-area-page $FN.svg && \

# probably redundant declaration guard
printf \
"#ifdef __cplusplus
extern \"C\" {
#endif

" > $TMP && \

# splash dimensions (static, for now)
printf \
"static const int splashWidth=480;
static const int splashHeight=320;

static const " >> $TMP && \

# hex array generation
xxd -i $FN.png >>  $TMP && \

# closing declaration guard
printf \
"#ifdef __cplusplus
}
#endif
" >> $TMP && \

mv $TMP $FN.h

#unconditional cleanup
rm -f $TMP $FN.png
