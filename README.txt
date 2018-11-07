This motley little collection is a snapshot of an ongoing experiment exploring
how to improve jack io in ZynAddSubFX. And anything else I feel like exploring.

See INSTALL for instructions.

Bon apetite.
------------
Changes
0.047 tidy a couple of minor issues on record and control change muting
0.046 re that subtly disturbing but oh so subtle difference in sound.
0.045 use pthread for managing SCHED_FIFO on threads
0.044 add alsa period size & samplerate control, and auto-record
0.039 - 0.043 dismissed as unfortunate medical outcomes
0.038 re-fix load parameters, flush record buffer at appropriate times, and
      attempt to reduce xrun impact of private moments (patch changes etc) 2009-11-12
0.037 Yoshimi records 2009-011-11
0.036 Will J Godfrey's load parameter file fix
0.034 Somewhat experimental in nature, adult supervision recommended: use fftw
      threaded routines. 2009-10-24
0.031 restore some sanity to the metering (perhaps). 2009-10-23
0.030 unlink jack midi semaphore on Close(). 2009-10-21
0.029 put the red line back in formant filter graph. 2009-10-19
0.028 the theory that started it all is debunked - revoke the reincarnation of
      the "three tries for lock" theory. Hardcore jack it is then. 2009-10-17
0.027 fix a little (weeks old) noteon velocity glitch. 2009-10-16
0.025/0.026 a couple of little optimisations. 2009-10-15
0.024 fix (ie disable) right-click instrument rename from main panel. It doesn't
      work in 2.4.0 either, not worth the bother. 2009-10-14
0.023 fix jack midi pitchbend, update master panel instrument label on bank slot
      rename. 2009-10-14
0.022 more bank management fixes 2009-10-13
0.021 fix right click instrument rename and a couple of bank management issues. 2009-10-13
0.020 a private matter, not for public dissemination. 2009-10-13
0.019 add commandline param for client name tag: -N <name tag>. 2009-10-09
0.018 tidy some "isshoos", and change alsa midi client name. 2009-10-08
