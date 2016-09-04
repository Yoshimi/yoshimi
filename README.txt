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


V1.3.9

Our new code name: Skylark

A major part of our work on this release is attempting to future-proof our code. Many distros are moving to GCC version 6, and code that built quite happily with older compilers is now rejected by the much more critical requirements.

While doing this, the very extensive testing also shook out some more obscure bugs which have of course been squashed.

However, amongst other improvements, we've split out roots and banks from the main config file and also created a new histories file. The separation means that the different functions can be implemented, saved and loaded, at the most appropriate time. These files have yoshimi as the doctype as they are in no way relevant to ZynAddSubFX.

The 'banks' file is saved every time roots, banks or instruments are changed, and again on a normal exit to catch current root and bank (which don't otherwise trigger a save).

The 'history' file is only saved on exit.

The 'config' file is only saved when you specifically call for it to be saved.

As well as recent Patch Sets, we now record recent Scales and recent States. Scales in particular had been requested by one of our users who composes with very different scale settings.

In the CLI prompt, when effects are being managed, the preset number is also shown at the prompt so you'll typically see something like:

yoshimi part 2 FX 1 Rever-7 >

Yoshim is now verified as being able to use 192000 Hz sample rate in both ALSA and Jack - if you have a suitable soundcard!

There have been a few minor GUI corrections and additions to the doc folder.

Many non-fatal system error messages can now be surpressed. this is particularly relevant for CLI use. This will be extended over time.
