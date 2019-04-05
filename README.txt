This motley little collection is a snapshot of an ongoing experiment exploring
how to improve jack io in ZynAddSubFX. And anything else I feel like exploring.

See INSTALL for instructions.

Bon apetite.
------------
Changes
0.058.1 switch fftw planner flag from FFTW_MEASURE back to FFTW_ESTIMATE,
        enabling plinky-plank to load in realistic time
0.058 all the rc5 fixes but lose the debug code;welcome Jeremy Jongepier's Dubstep bass.
0.057.2-rc5 fix a deadlock on loading instruments using PADsynth.
0.057.2-rc4 more testing of issues.
0.057.2-rc3 Further adjustment to xml file reads.
0.057.2-chk2 Reinstate 0.057 interrupt fix.
0.057.2-chk1 Possible fix for intermittent instrument load failures.
0.057.1 Fix inverted reverb panning.
0.057 reinstate jack midi pitchbend fix (from circa 0.022???); incorporate
      Nedko's lazy signal response fix; check for sse availability, and step
      over things accordingly. 
0.056 reinstates PADsynth functionality - a stuff up in XMLwrapper::getparbool(). 
0.055.6 possibly fixes loading compressed xml. 
0.055.5 Add airlynx/chip instruments.
0.055.4 Drop double buffer fltk init; add --no-gui
0.055.3 use the correct #include <FL/x.H> and move it from MasterUI.fl to GuiThreadUI.fl
0.055.2 add param file loaded from command line to params history.
0.055.1 check for (and require) alsa >= 1.0.17.
0.055 pre5 becomes 0.055, no change.
0.055-pre5 a jack midi bug fix; some subtle performance enhancers including
           slightly more granular locking. 
0.055-pre4 gui ignition sequence adjusted. 
0.055-pre3 a few gui fixes, maybe even fixed the gui startup issue; LADI 1 support seems ok.
0.055-pre2 shakes a fist at the gui startup failure; getopt replaced by argp.
0.055-pre1 shuffled the master gui display about a bit; LADI 1 support seems ok.
0.054.1 attitudinal adjustment to a few more pans. 
0.054 a few things from pre4 fixed; ladi1 SIGUSR1 handler in place but still
      untested in ladish context; interrupt handler now handles rude
      interuptions better; instrument banks now shipped uncompressed; mostly
      harmless?
0.054-pre4 First cut LADI Level 1 compliance. Some new code, and quite a bit of
           old code moved around, so expect issues. 
0.054-pre3 formed an alliance with the random_r family; dropped no gui option. 
0.053.3 fix NUM_KIT_ITEMS, accidentally reduced 16 -> 3; improve handling of
        rtprio availability on thread creation;       
0.053.2 set the priority of threads in accordance with jack's firm recommendation
0.054-pre2 fixed random error in randomness; more menu madness.
0.054-pre1 improved xmz file selection, including persistent history selection;
           prune some extraneous code;
0.053 no change from pre4
0.053-pre4 reinstate 'last bank' recall
0.053-pre3 sort out the oversights from recorder fixes:- settings corruption;
           stray mushroom clouds from recorder
0.053-pre2 utter rubbish
0.053-pre1 reverted to 0.045, and just added the sane stuff; recording should
           be fixed; missing ~/.yoshimiXML.cfg no longer a drama 
0.046 to 0.051 should be considered as just unfortunate medical outcomes
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
