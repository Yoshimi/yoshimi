
objects = Alienwah.o    Chorus.o      Echo.o    Effect.o \
          EffectLFO.o   EffectMgr.o   Phaser.o  Reverb.o \
          EQ.o          Distorsion.o  DynamicFilter.o

all: $(objects)

-include ../Make.deps

.PHONY : clean
clean: 
	rm -f $(objects)
	rm -f makeinclude.deps

