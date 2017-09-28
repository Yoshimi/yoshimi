V 1.5.4 - Blackbird

This is a very small release feature-wise but with a lot of cleanups in the code.

It is now possible to MIDI-learn *all* rotary controllers. Also there is a new one in the small MIDI controls window. This is master bandwidth (CC75) and is most effective on instruments with a rich set of harmonics - especially SubSynth.

Some of the tooltips were a bit ambiguous, and these have been changed to be more obvious.

We now include "The Yoshimi Advanced User Manual" as a PDF, which when installed will be placed in /usr/local/share/yoshimi. If you have a preferred PDF reader installed, then you can fetch it from the 'Yoshimi' dropdown menu.


V 1.5.3 - Swift

We have revised the whole of Microtonal (scales) for better accuracy, and fixing originally incorrect range limitations. This is now much closer to the Scala specification, although there seems to be an ambiguity when you have a keymap defined and have an inverted scale - which key is the pivot point?

The GUI representation has been improved so you always see what you entered, not an approximation with strings of '9's!

Microtonal settings are now fully accessible to the CLI.


Vector control has had a makeover. Amongst other things, the name field is now editable and is stored when you save. This means it will be retained on patch sets and states as well, so when these are reloaded you will know what vectors are embedded.

An entire state file can now be loaded silently in the same way as patch sets and vectors can.

Further improvements to the CLI are full access to all configuration settings, and better organisation of command grouping and help lists.


Some data was not ordinarily saved if features were disabled at the time of saving. We have added a switch to allow all data to be saved regardless. This makes for larger files of course, but does ensure that you can get an *exact* recovery if you need it.


You can now directly interact with the formant filter graphic display in a way that is more intuitive and easier to use.

The Console window now scrolls the right way! One of yoshimi's little helpers worked out how to scroll the window to keep the most recent line visible at the bottom.

It is now possible to run Yoshimi stand-alone with both GUI and CLI input disabled, thus responsive only to MIDI input. In view of this we have added a new shortform NRPN that will shut it down cleanly. You simply send 68 on both NRPN CCs (99 and 98).


Under the hood:

As well as additional Gui controls transferred to the new lock-free system, some of the earlier implimentations of CLI controls have been transferred. One result is that much of the code is leaner, and easier to follow.

Some needlessly dynamic memory allocations have now been changed to fixed ones. This gives a noticable reduction to DSP peaks.

There are a few more old and new bugfixes. These days, there seem to be more new ones than old ones. In a sense this is actually good news.


V 1.5.2 - Goldfinch

We now implement the fairly new LV2 state:StateChanged URI
This means any change you make in the Yoshimi GUI will be reported to the host.


MIDI-learn improvements:

The current learned list is now included in state saves.

NRPNs can now be set as 7bit (LSB only). This is used by some hardware synths and controllers.

Min and Max values are now shown as percentages and have a resolution of 0.5%

Much of the controllers window, and quite a lot of 'switches' (such as engine enables, kit item mutes) are now learnable and act silently. Most of these are 'next note'.

Learnable checkboxes have a pale blue surround (a similar colour to rotary controls).

Learnable Menus and Selectors have their arrows the same blue colour.

Note. To learn menus you need to make an actual selection with the right mouse button while holding down the Ctrl key.


Under the hood:

More Gui controls transferred to the new lock-free system.

The usual round of ancient and modern bugfixes.


V 1.5.1 - Jenny Wren

MIDI-learn has been extended so that you can now learn aftertouch, the pitch wheel (to full 14 bit resolution) and most recently NRPNs. A number of hardware devices send these for greater control depth and to provide more than the usual number of controls. Also, there is a 'Settings' option to always open the editing window on a sucessful 'learn'.

Main volume now has interpolation so there is no zipper noise, and part volume and pan have better interpolation for the same reason.

LFO frequency and depth are now fully dynamic.

Four common MIDI controls now have a window (right click on 'Controllers'). This is so that they can be be MIDI-learned. The reason you might want to, is for linking them to aftertouch - a particular benefit to users of wind controllers.

A number of actions are now 'soft' in that a fast fade is performed, then the synth is disabled, the action takes place, and the synth is re-enabled.
These are 'Stop' and 'Reset' as well as loading Vectors and Patchsets.

The 'Solo' feature now ignores a value of zero if it is in 'Loop' mode. This means you can use a simple on/off switch control to step through the parts only when it gets the 'on' value.

Visually there have been a lot of improvements. Main window key shortcuts work correctly! General layouts have been further harmonised. The AddSynth voice list window is now fully in sync with the voice windows themselves.

More tooltips give real values on hovering. Some also give small graphic representations.

We have a new splash screen, which also doesn't block anymore. There is now also a 'Util' directory which has the tools, instructions and examples to enable you to create a personalised splash.

There is more information for making custom builds.

Both CLI and GUI have been further isolated from the audio thread, and there is now intelligent buffering of incoming MIDI where needed.


V 1.5.0 - The Robin

Vector control now has its own dedicated button on the main window.

Any attempt to set an invalid bank root will be ignored. The same was already true for banks themselves. Also, on first time startup, discovered roots will be given ID numbers starting from 5 and in steps of 5.

If working from the command line, listing roots and banks will identify the current ones with a '*'.

Channel switching has now matured to a 'Solo' feature accessed from the mixer panel.

Another new feature is one that has been asked for several times - a crossfade for overlapping note ranges in instrument kits.

Filter tracking could never quite reach 100% so if using it to get 'notes' from noise it would go slightly out of tune. Well, now we have an extra check box that changes its range so that instead of -100% to 98.4% it will track 0% to 198.4%

Many of the controls now have active tooltips that show the current value when you hover over them. Also, many have real-world terms. dB, Hz, mS etc.

Finally, we have very full featured MIDI learn capability. It can be reached from the 'Yoshimi' drop-down menu, and tooltips will remind you of how it is used.
