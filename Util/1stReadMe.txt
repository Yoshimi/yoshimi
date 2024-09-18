The following are utility programs and files that are outside the main Yoshimi build but can be run from a terminal window to add to, or modify some features.

The file 'ControlModel.svg' is the source file for the PDF version in dev_notes. It is somewhat out of date!

MakeSplash.sh
Command:
sh <path>MakeSplash.sh <imagefile>

This converts any .svg or .png image to a hex dump and embeds it in the Yoshimi source so that on the next 'make' it will become the new splash screen. SVGs are preferred as they can be easily edited.

The file 'splashdefault.png' is the oldest released screen - it was created before SVG capability was available. 'YoshimiSplash.svg' is one created by Jesper that can be used as a template, and is the current one.

You will need to give the full filepath for both "MakeSplash.sh" and your image file.


midiListgen.cpp
This is a source file used to build a program that generates an HTML formatted list of MIDI note names, numbers and frequencies. It is highly accurate and was used to generate the list used in the guide.


incBuildNumber.py
This is a python program that picks up the current build number, increments it and resaves it.


updateGuideVersion.py
Thi is a python program that reads the current Yoshimi version details and extracts just the number, discarding any suffix, then embeds it in the HTML user guide. It does this in a manner that doesn't change the file size.


Three bash scripts in parent directory:
    comp    enters the build directory, compiles the code, then returns to the project directory.

    run     enters the build directory, runs the compiled code, then returns to the project directory.

    set     enters the Util directory, runs updateGuideVersion.py, then runs incBuildNumber.py, then enters the build directory, compiles the code and remains in the build directory.
