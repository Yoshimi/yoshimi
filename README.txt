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


V1.3.8

We have our first code name: The Swan

MIDI program changes have always been pretty clean from the time Cal first introduced them, but now GUI changes are just as clean. While it is generally best to change a program when the part is silent, even if a part is sounding there is usually barely a click. There is no interference at all with any other sounding parts.

Sometimes MIDI CCs don't seem to give the results you expect. Well, there is now a setting that will report all incoming CCs so you can discover what Yoshimi actually sees (which may not be what you were expecting).

At the request of one of our users, we have now implemented CC2, Breath Control.

The 'Humanise' feature has had more interest so it's been upgraded. It's now a slider and it's setting can be saved in patch sets. It provides a tiny per-note random detune to an *entire* part (all engines in all kits), but only to that part.

Audio & Midi preferences have been improved. If you set (say) ALSA MIDI and JACK audio, either from the GUI or the command line, the setting can be saved and will be reinstated on the next run. These settings are per-instance so if you have multiple sound cards you can make full use of them.

Barring major system failures, there are now no circumstances where Yoshimi will fail to start.

There is greater control of your working environment. You can have just the GUI, just a CLI or both, and these settings can be saved. If you try to disable both you will get a polite warning and will be left with just the CLI.

The CLI can now access almost all top level controls as well as the 'main page' part ones and can select any effect and effect preset, but can't yet deal with the individual effects controls. It can be used to set up Vector Control much more quickly and easily than using NRPNs.

It is also context sensitive, which along with careful choice of command names and abreviations allows very fast access with minimal typing.

Yoshimi's parser is case insensitive to commands (but not filenames etc.) and accepts the shortest unambiguous abbreviation. However it is quite pedantic, and expects spelling to be correct regardless of length. Apart from the 'back' commands, it is word-based so spaces are significant.

The CLI prompt always shows what level you are on, and the help lists are also partly context sensitive so you don't get a lot of irrelevent clutter.

There is more, and a lot more to come!


While doing all this work, we've alse ensured that Yoshimi instrument patches are still fully compatible with Zyn ones, and have now ported across the new refinements with thanks.
