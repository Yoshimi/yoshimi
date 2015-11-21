Cal's last entry here is preserved in doc/Histories


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



In Yoshimi V1.3.5 a number of existing, as well as new features have come together to give much greater flexibility (especially for automation) using standard MIDI messages. These are:

NRPNs.
Independent part control.
16, 32 or 64 parts.
Vector Control.
Direct part stereo audio output.


NRPNs can handle individual bytes appearing in either order, and usually the same with the data bytes. Increment and decrement is also supported as graduated values for both data LSB and MSB. Additionally, ALSA sequencer's 14bit NRPN blocks are supported.


Independent part control enables you to change instrument, volume, pan, or indeed any other available control of just that part, without affecting any others that are receiving the same MIDI channel. This can be particularly interesting with multiply layered sounds. There are more extensions planned.


With 32 and 64 parts it helps to think of 2/4 rows of 16. When you save a parameter block the number of parts is also saved, and will be restored when you reload.
By default each *column* has the same MIDI channel number, but these can be independently switched around, and by setting (say) number 17 taken right out of normal access.

In tests, *compiling* for 64 parts compared with 16 parts increased processor load by a very small amount when Yoshimi was idling, but this becomes virtually undetectable once you have 8+ instruments actually generating output. In normal use, selecting the different formats makes no detectable difference but using the default 16 reduces clutter when you don't need the extra.


Vector control is based on these columns giving you either 2 (X only) or 4 (X + Y) instruments in this channel. Currently the vector CCs you set up can (as inverse pairs) vary any combination of volume, pan and filter cut-off. More will be added.
To keep the processor load reasonable it pays to use fairly simple instruments, but if you have sufficient processing power it would be theoretically possible to set up all 16 channels with quite independent vector behaviour!


Direct part audio is Jack-specific and allows you to apply further processing to just the defined part's audio output (which can still output to the main L+R if you want). This setting is saved with parameter blocks. Currently it is only set in the mixer panel window, but it will also eventually come under MIDI direct part control.
Again, to reduce unnecessary clutter, part ports are only registered with Jack if they are both enabled, and set for direct output. However, once set they will remain in place for the session to avoid disrupting other applications that may have seen them.
