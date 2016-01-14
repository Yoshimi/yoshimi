Cal's last entry here is preserved in doc/Histories

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

V1.3.7

This just a minor release focussing on usability details.

Yoshimi now remembers most window positions on a per instance basis, and will re-open them at the same locations. Also, if you shut down with these windows open, they will be opened again on the next run.

Almost all window titles carry the instance number as well as details such as part number/name, engine, and whether it is kit mode. For addsynth oscillator, the voice number is included. Bank and Instrument windows, as before, show the MIDI ID numbers along with the complete file paths.

The top menu bar has been reorganised for better grouping. Mixing unrelated operations on a single menu is never a winner.

A complete clear-down now has it's own button, 'Reset'. This is as close as possible to a restart, but works independently for each instance. Re-aranging this group of buttons into two rows means they can be longer - "Virtual Keybd" is a bit more obvious than "vrKbd".

The 'Controllers' window now (at last) correctly updates when you hit its reset button.

Other window layouts have had a bit of polish.

There is now an option so you can see the version details of all XML files in the 'Console' window as they are loaded.

Command line access is now more than just a proof of concept. It has a half-decent parser, paging and history. All these are developing alongside considerably more controls.

V1.3.6

Principal features for this release are the introduction of controls from the command line, covering many stetup options, as well as extensive root/bank/instrument management. Some of these new controls are also available to MIDI via new NRPNs.

Vector control has been extended so that there are four independent 'features' that each axis can control,

ALSA audio has had a makeover and now can work at your sound card's best bit depth - not just 16 bit (as it used to be).

In the 'examples' directory there is now a complete song set, 'OutThere.mid' and 'OutThere.xmz'. Together these produce a fairly complex 12 part tune that makes Yoshimi work quite hard.

More information on these and other features are in the 'doc' directory.


We have a new policy with version numbering. If the Version string contains a 4th number, then this is purely a bugfix version and will have no changed features. If you are having problems you may want to upgrade.

1.3.5 is the current version
1.3.5.1 is the first bugfix
This has one minor GUI fix, and a fix for uses of fltk V1.1 with build problems.

V1.3.5

In response to suggestions at LAC 2015 we have made the title bars of all editing windows display both the part number and the current name of the instrument you are working on. In the addsynth oscillator editor you also see the number of the oscillator you are editing.


Also, in response to suggestions, horizontal as well as vertical mouse dragging can be used to set rotary controls. Additionally, the mouse scroll wheel can be used, and if you hold down the 'ctrl' key you can get very precise setting.


Another request we had was for the part effects window to have the same layout as System and Insertion effects. This has been done and it is now almost identical to Insertion effects.


The most noticeable GUI enhancement is colour coded identification of an instrument's use of Add Sub and Pad synth engines, no matter where in the instrument's kit they may be. This can be enabled/disabled in the mixer panel. It does slow down yoshimi's startup, but due to the banks reorganisation (done some time ago) it causes no delay in changing banks/instruments once you are up and running.
Some saved instruments seem to have had their Info section corrupted. Yoshimi can detect this and step over it to find the true status. Also, if you resave the instrument, not only will the PadSynth status be restored, but Add and Sub will be included, allowing a faster scan next time.
