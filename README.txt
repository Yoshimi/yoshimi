V 1.5.8 - Kingfisher
Bright flash of colour.

The most significant changes in this release are all usability ones.

At the request of one user (and agreement of several others) some of the microtonal settings are now MIDI-learnable, and appropriately highlighted.

The CLI can now open and close instances, and switch between them.

There is a new 'Solo' type 'TwoWay' this works in a similar way to 'Loop', but (apart from zero) values less than 64 step from right to left, instead of the other way round. The highlighting is also a bit better. Both 'Loop' and 'TwoWay' also have debounce protection of approximately 60mS.

The CLI can now clear a part's instrument. A fairly obvious option that somehow got missed.

In the Banks windows Instruments can now be swapped between banks and bank roots. Banks can also be swapped between roots. This is an extension using exactly the same controls as those already available for in-bank swaps.

The latest feature is autoloading instances. With this enabled, any instance that was open when the main one is closed will be re-opened on the next run. If these instances were set for starting with their default state, then all those settings will be performed. Therefore, a very comprehensive and detailed session can be started with a single command!

A lot of the documentation supplied with Yoshimi has been updated, including the Advanced User Manual.

