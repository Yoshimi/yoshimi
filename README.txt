V 1.5.0 - The Robin

Vector control now has its own dedicated button on the main window.

Any attempt to set an invalid bank root will be ignored. The same was already true for banks themselves. Also, on first time startup, discovered roots will be given ID numbers starting from 5 and in steps of 5.

If working from the command line, listing roots and banks will identify the current ones with a '*'.

Channel switching has now matured to a 'Solo' feature accessed from the mixer panel.

Another new feature is one that has been asked for several times - a crossfade for overlapping note ranges in instrument kits.

Filter tracking could never quite reach 100% so if using it to get 'notes' from noise it would go slightly out of tune. Well, now we have an extra check box that changes its range so that instead of -100% to 98.4% it will track 0% to 198.4%

Many of the controls now have active tooltips that show the current value when you hover over them. Also, many have real-world terms. dB, Hz, mS etc.

Finally, we have very full featured MIDI learn capability. It can be reached from the 'Yoshimi' drop-down menu, and tooltips will remind you of how it is used.


V 1.4.1
Lyrebird

First of all, we have a new quick guide that's in Yoshimi's 'doc' directory. It's just something to help new users get started.

We've always logged warnings if it wasn't possible to run either audio or MIDI, but now we also give a GUI alert.

From this version onward it is possible to autoload a default state on startup, so you see Yoshimi already configured exactly as you like, with patches loaded and part destinations set.

To make it easier to position patch changes in a running MIDI file, there is a new option to report the time these take to load.

Vector control settings are now stored in patch set and state files.

We implemented a simpler way to perform channel switching so the 'current' MIDI instrument can seem to be changed instantly, retaining the note tails of the previous one.


V 1.4.0
Diamond Dove

About 18 months ago vector control became a 'thing' for Yoshimi and later, was first demonstrated to a handful of people at LAC 2015. At that time it was only accessible via NRPNs. Then about 6 months ago it became available to command line users as well, and 3 months ago vector saving and loading became possible.

Now however, there is a shiny new window so the poor disadvantaged pointer-pushers can also have full control. This is currently reached via the 'Yoshimi' tab, but we may move it to its own button. Also, saving and loading vectors is now preserved in the recent histories file so you can quickly restore these - the same as you can for saved patch sets, scales and states.

Vector entry via NRPNs and the CLI have also been upgraded slightly, so they will now automatically set the right number of parts available, and enable the required ones with the correct incoming channel number.


A new concept is shortform NRPNs. This is where instead of the NRPN setting up for data entry of values, the NRPN is of itself the entire command. With a suitable sequencer like Rosegarden, you start with a two byte value, then only need to enter single byte CCs to change the setting.
See doc/Shortform_NRPN.txt

This came about through discussion after my demo of channel switching on MiniLAC2016. The idea was liked, but having to mess about with multiple NRPNs and their data settings was a fiddle.


Some other usability enhancements:

Some people don't like our splash window - Boo!
You can now disable it in settings {mutter}{mutter}

Many people didn't realise there were two types of resonance interpolation, determined by whether you click the left or right mouse button. This has now been split into two with better tooltips.

Jack audio autoconnect is now configurable in the GUI and stored so you don't have to set it with a startup argument.

In MIDI settings you can now tell Yoshimi to ignore the 'reset all controllers' message - various bits of hardware and software can send these at the most inappropriate times.

A right click on a button for a child window now closes the parent and a right click on that child's close button re-opens the parent. Use this a few times and you'll wonder how you managed without. Actually, this has been possible for a long time with Root/Bank/Instrument windows :)

A right click on the track of any slider, or on any rotary knob will return it to its home position.

There has been some shuffling in the GUI to make the different windows more consistent and easier to recognise. This has also enabled us to increase the size of the smallest control knobs. Most sliders are now indented - it makes them more obvious.

Scroll wheel behaviour on both knobs and sliders has also been adjusted to be more consistent. By itself movement is pretty fast, but hold down the ctrl key, and you'll get very fine resolution.

Other matters:

Yoshimi now has a build number, and this appears in the startup log. That's probably only of interest to those building the master, or for reporting bugs - whatever they are :P

Actually, there are the usual bugfixes (ancient and modern) and we've also made a small improvement in the way we handle an all-jack environment.

The compatibility work we did for V1.3.8 ensured the all-important instrument files were correct, but we didn't have time to implement all of the controls. These are now in place.

Currently there is quite a lot of preparatory work under way but its not ready for prime time. It's still really proof of concept.
