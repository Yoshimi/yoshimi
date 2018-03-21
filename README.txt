V 1.5.7 - Nighingale

This release is mainly focussed on usability improvements.

For example, the Master Reset can now clear MIDI-learn lists. All you have to do is hold down the Ctrl key when clicking the button. Usually you don't want to, but it's there for the times you do.


There are rare occasions when a very large PadSynth patch can take as much as 15 seconds to initialise. If you try to do anything on that part that can affect PadSynth (during that time) the result can be fatal. Well not any more, as now you'll get a warning message:
"Part {n} busy."


In the bank selection window there are two new buttons 'Import' and 'Export'. These allow you to transfer complete banks in and out in a controlled manner.

There are a lot of these out in the wild, and the 'Import' process lets you copy them in to specific slots in particular roots. It only copies in what Yoshimi recognises, but informs you if there are any unrecognised ones. There is sometimes really odd junk that has crept into 'wild' banks. Also, Yoshimi will never overwrite any of your precious installed banks or mess with their IDs.

Similarly when you want to pass one of your own banks to friends, with 'Export' it is easy to identify the bank you want to copy out, and Yoshimi will not overwrite any external directories.


Following on from this, many people don't fill in the Author and Copyright fields of their own instrument patches, making it hard to acknowledge them.  This is now partly automated in the hope that it may encourage people to do so.

You can set up this default by going to the 'Instrument Edit' window and filling the field in just once, then while holding down the Ctrl key, click on the 'Default' button. From then on, just a click on this will fill the field with this text.


Going further into instruments, we now have a few useful key shortcuts.

If you hold down 'K', and click on the 'Edit' button in the main window, it will directly open the kit editor window for the current part. Similarly holding down 'E' and clicking will give you the part's effects window.

Most voice patches only use the first Add, Sub and Pad engines and there are similar shorcuts for these using 'A', 'S' & 'P' and clciking on the 'Edit' button. If you use the left mouse button it will only open the associated window if that engine is enabled, otherwise it will open the usual Part Edit window. Using the Right button will enable the engine then open its window.

For the QWERTY keyboarders 'D' can be used instead of 'P', putting these in a neat row :)


Now we take a dive into the feared AddSynth Voice window!

The first thing you notice is these are tabbed, so you can quickly jump between the voices. An extra detail is that non-active voices have their numbers in light grey text, while the active (or the current selected) ones have clearly visible black text.

This is all fully in sync with the Kit Edit window.


We've changed some ambiguous wording in the Voice window too. The 'Change' buttons are now 'Waveform' ones. Also the highly confusing 'Ext' or 'Internal' entries are better worded - it was quite common for people to ask how they got the 'Ext' ones and where they came from.

For the voice itself, the selector is now named 'Oscillator' and the text is 'Internal' or 'Voice {n}'

At the top of the Modulator section, you have 'Oscillator Source' and this time it will be 'Local' or 'Voice {n}'.

The lower selector is titled 'Local Oscillator' and the text will be 'Internal' or 'Mod. {n}'.

An extra detail is that these entries are greyed out for voice 1, as you can only select the internal oscillator or one from a lower numbered voice.


Returning to effects, there is now interpolation in place for the controls, which makes these crackle-free and far more useful with MIDI-learn - Remember, you can have up to 200 learned lines.


Finally, there are the usual code improvements and bug fixes.
