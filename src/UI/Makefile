
%.cc  : %.fl
	fluid -c $<

objects =

ifneq ($(DISABLE_GUI),YES)
    objects = WidgetPDial.o   PresetsUI.o  EnvelopeUI.o \
              LFOUI.o         FilterUI.o   VirKeyboard.o \
              ConfigUI.o      SUBnoteUI.o  ResonanceUI.o \
              OscilGenUI.o    ADnoteUI.o   PADnoteUI.o \
              EffUI.o         BankUI.o     PartUI.o \
              MicrotonalUI.o  MasterUI.o   GuiThread.o
endif

all: $(objects)

GuiDependencies: MasterUI.h
	@rm -f ../Make.deps
	@sh -c "$(CXX) -MM -MG -w *.cc >> ../Make.deps"

WidgetPDial.o: WidgetPDial.fl WidgetPDial.h WidgetPDial.cc

PresetsUI.o: PresetsUI.fl PresetsUI.h PresetsUI.cc

EnvelopeUI.o: EnvelopeUI.fl EnvelopeUI.h EnvelopeUI.cc

LFOUI.o: LFOUI.fl LFOUI.h LFOUI.cc

FilterUI.o: FilterUI.fl FilterUI.h FilterUI.cc

ResonanceUI.o: ResonanceUI.fl ResonanceUI.h ResonanceUI.cc

OscilGenUI.o: OscilGenUI.fl OscilGenUI.h OscilGenUI.cc

ADnoteUI.o: ADnoteUI.fl ADnoteUI.h ADnoteUI.cc

SUBnoteUI.o: SUBnoteUI.fl SUBnoteUI.h SUBnoteUI.cc

PADnoteUI.o: PADnoteUI.fl PADnoteUI.h PADnoteUI.cc

EffUI.o: EffUI.fl EffUI.h EffUI.cc

BankUI.o: BankUI.fl BankUI.h BankUI.cc

PartUI.o: PartUI.fl PartUI.h PartUI.cc

VirKeyboard.o: VirKeyboard.fl VirKeyboard.h VirKeyboard.cc

ConfigUI.o: ConfigUI.fl ConfigUI.h ConfigUI.cc

MicrotonalUI.o: MicrotonalUI.fl MicrotonalUI.h MicrotonalUI.cc

MasterUI.o: MasterUI.fl MasterUI.h MasterUI.cc

GuiThread.o: GuiThread.fl GuiThread.h GuiThread.cc

.PHONY : clean
clean: 
	rm -f $(objects)
	rm -f makeinclude.deps
	rm -f *.h *.cc
