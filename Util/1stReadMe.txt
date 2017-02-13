These are utility programs and files that are outside the main Yoshimi build but can be run from a terminal window to add to, or modify some features.


MakeSplash.sh
Command:
sh MakeSplash.sh <imagefile>

This converts any .svg or .png image to a hex dump and embeds it in the Yoshimi source so that on the next 'make' it will become the new splash screen. SVGs are preferred as they can be easily editied to include version numbers etc.

The file 'splashdefault.png' is the current released screen - it was created before SVG capability was available, and 'splashreference.svg' is the slightly modifed form of one created by Jesper that can be used as a template.

You will need to give the full filepath for both "MakeSplash.sh" and your image file.
