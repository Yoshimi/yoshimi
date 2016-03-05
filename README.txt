Cal's last entry here is preserved in doc/Histories

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
