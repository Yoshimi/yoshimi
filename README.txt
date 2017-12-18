V 1.5.6 - Fieldfare

Our new BSD friends turned up a few issues that don't seem to affect the common Linux distros, and a couple of fairly obscure bugs surfaced. These would have prompted a bugfix release. However, at the same time two new features were requested that are quite useful and easy to implement.

In MIDI-learn, if you set Max and Min to the same value this becomes a threshold and anything equal or lower behaves as if the input was 0 while anything higher behaves as if it was 127.

Breath control (CC2) can now be disabled on a per-instrument basis. It seems some MIDI controllers send this on joystic movements.


V 1.5.5 - Mistle Thrush

Some users wanted a way to store the Controllers settings with an instrument. These can make quite a dramatic difference to the sound. There is now a superset of instruments that can be saved instead of, or as well as, the standard ones. On loading, Yoshimi will look for the extended version first. This applies to instruments in banks as well as externally saved ones. If you have an extended type loaded the instrument name will be a mid-blue instead of black. This is refleced in the stored instruments in bank slots.

From now on any improvements we make to instrument patches will be applied in full to the extended version, and as much as is viable (in a compatible form) to the standard ones. Under no circumstances will we change standard patch format. There are many hundreds of these in the wild, and musicians may have their own reasons for preferring them (and older versions of the synth).


There was a strange, really ancient bug in 'legato' where if you fumble and hit two keys pretty much together you end up with alternate silent notes until you lift all keys. This has now been comprehensively fixed.


While we concentrate on larger issues we don't neglect the smaller ones.

For example: You can't have legato mode and drum mode at the same time, but this hasn't been obvious. From now on, if you have drum mode set then try to set legato, you'll still see the setting, but as drum mode takes priority, the legato text will be shown in red. Cancel drum mode (making legato valid) and the text will turn black again.

If you are using a legato MIDI pedal, Yoshimi's part 'mode' will show this change, and again will turn red if an instrument with drum mode is on that part.


Load and save dialogues intelligently recognise the history lists and offer the appropriate first choice. External instrument loads and saves are now also remembered.

For saves, on a fresh start you will offered your home directory regardless of where yoshimi has been launched from, but uniquely, in the case of saving external instruments you will always be offered the name of the instrument in the currently selected part - prefixed with the home directory.

There is now a specific menu item in 'State' (Save as Default) for saving the current complete setup as the default. This will always be saved to Yoshimi's config directory and will not show in history lists.

If "Settings...->Switches->Start With Default State" has been set, and a default state has been saved, not only will a complete restart load this, but a master 'Reset' will load this instead of doing a first-time default reset.

A final detail with the history lists is that in each list type, the last used item will be placed at the top of the list. This is especially useful when you want to continually save/load an item you are currently working on.


When first implemented MIDI-learn was limited conservatively to 128 lines. With experience of its actual performance this has now been increased to 200.


The CLI has had attention too. A few more controls have been enabled, and the existing ones smartened up and made more consistent. As an aside, there is a new experimental branch with an interface that *only* works via the CLI - not ready for prime time yet.


Yoshimi can now run happily with jack on BSD systems.


Techie bits.
There are a lot more minor optimisations throughout the code. This has resulted in an overall drop in code size, as well as some critical operations being slightly faster.

The last few contentious parameter changes have now been made thread safe, and by default we run with NO mutex locks (the calls are there but return 'empty'). There is a single queue that all settings pass through so there should be no possibility of interference... Well, that's the theory :)

CMakeLists.txt now has a specific option for older X86 processors. Also, if none of the build options are set then not even sse extensions will be included. How Yoshimi will handle on such an old processor is left as an exercise for the student :P


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
