This motley little collection is a snapshot of an ongoing experiment exploring
how to improve jack io in ZynAddSubFX. And anything else I feel like exploring.

See INSTALL for instructions.

Bon apetite.
------------
Changes
0.016  Alsa audio bug fixes, plus fixed an issue with potential to corrupt
       ~/.zynaddsubfxXML.cfg on saving settings . On start up, it now tries to
       load ~/.yoshimiXML.cfg. If it can't find that it tries
       ~/.zynaddsubfxXML.cfg. On exit it now writes to ~/.yoshimiXML.cfg
0.015  Resonance fix (ref Will J Godfrey Sharp Synth)
0.014  AlsaEngine tidies
0.013  Free jack midi ringbuffer on close.
0.012  cmake build fixes
0.011  cmake build fixes
