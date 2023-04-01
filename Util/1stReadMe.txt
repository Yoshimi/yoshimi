These are utility programs and files that are outside the main Yoshimi build but can be run from a terminal window to add to, or modify some features.


MakeSplash.sh
Command:
sh <path>MakeSplash.sh <imagefile>

This converts any .svg or .png image to a hex dump and embeds it in the Yoshimi source so that on the next 'make' it will become the new splash screen. SVGs are preferred as they can be easily editied.

You will need to give the full filepath for both "MakeSplash.sh" and your image file.


midiListgen.cpp
This is a source file used to build a program that generates an HTML formatted list of MIDI note names, numbers and frequencies. It is highly accurate and was used to generate the list used in the guide.

