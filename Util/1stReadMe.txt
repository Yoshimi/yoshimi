The following are utility programs and files that are outside the main Yoshimi build but can be run from a terminal window to add to, or modify some features.

The file 'ControlModel.svg' is the source file for the PDF version in dev_notes.

MakeSplash.sh
Command:
sh <path>MakeSplash.sh <imagefile>

This converts any .svg or .png image to a hex dump and embeds it in the Yoshimi source so that on the next 'make' it will become the new splash screen. SVGs are preferred as they can be easily edited.

The file 'splashdefault.png' is the oldest released screen - it was created before SVG capability was available. 'YoshimiSplash.svg' is one created by Jesper that can be used as a template, and is the current one.

You will need to give the full filepath for both "MakeSplash.sh" and your image file.


midiListgen.cpp
This is a source file used to build a program that generates an HTML formatted list of MIDI note names, numbers and frequencies. It is highly accurate and was used to generate the list used in the guide.


switch_time.cpp
This is a source file used to test the behaviour of large switch statements. The difference is significant, and more noticeable on slower single core processors. Compiling with all optimisations on makes and even greater difference.

Updated to suppress compiler warnings.
