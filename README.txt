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

To build yoshimi fetch the tarball from either:
    http://sourceforge.net/projects/yoshimi
Or:
    https://github.com/Yoshimi/yoshimi

Our user list archive is at:
    https://www.freelists.org/archive/yoshimi


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


V 1.4.1
Lyrebird

First of all, we have a new quick guide that's in Yoshimi's 'doc' directory. It's just something to help new users get started.

We've always logged warnings if it wasn't possible to run either audio or MIDI, but now we also give a GUI alert.

From this version onward it is possible to autoload a default state on startup, so you see Yoshimi already configured exactly as you like, with patches loaded and part destinations set.

To make it easier to position patch changes in a running MIDI file, there is a new option to report the time these take to load.

Vector control settings are now stored in patch set and state files.

We implemented a simpler way to perform channel switching so the 'current' MIDI instrument can seem to be changed instantly, retaining the note tails of the previous one.
