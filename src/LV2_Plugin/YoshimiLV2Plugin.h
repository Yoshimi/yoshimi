/*
    YoshimiLV2Plugin

    Copyright 2014, Andrew Deryabin <andrewderyabin@gmail.com>
    Copyright 2020-2021, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef YOSHIMI_LV2_PLUGIN_H
#define YOSHIMI_LV2_PLUGIN_H

#include "lv2/lv2plug.in/ns/ext/instance-access/instance-access.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "lv2extui.h"
#include "lv2extprg.h"

#include <sys/types.h>
#include <string>
#include <vector>
#include <semaphore.h>

#include "Misc/SynthEngine.h"
#include "Interface/InterChange.h"
#include "Interface/Data2Text.h"
#include "Interface/RingBuffer.h"
#include "MusicIO/MusicIO.h"

class YoshimiLV2Plugin : public MusicIO
{
private:
   SynthEngine *_synth;
   uint32_t _sampleRate;
   uint32_t _bufferSize;
   std::string _bundlePath;
   LV2_URID_Map _uridMap;
   LV2_Atom_Sequence *_midiDataPort;
   LV2_Atom_Sequence *_notifyDataPortOut;
   LV2_URID _midi_event_id;
   LV2_URID _yoshimi_state_id;
   LV2_URID _atom_string_id;
   LV2_URID _atom_long;
   LV2_URID _atom_float;
   LV2_URID _atom_type_chunk;
   LV2_URID _atom_type_sequence;
   LV2_URID _atom_state_changed;
   LV2_URID _atom_object;
   LV2_URID _atom_blank;
   LV2_URID _atom_event_transfer;
   LV2_URID _atom_position;
   LV2_URID _atom_bpb;
   LV2_URID _atom_bar;
   LV2_URID _atom_bar_beat;
   LV2_URID _atom_bpm;
   uint32_t _bufferPos;
   uint32_t _offsetPos;
   sem_t _midiSem;

   struct midi_event {
       uint32_t time;
       char data[4]; // all events of interest are <= 4bytes
   };

   float _bpm;

   float *_bFreeWheel;

   pthread_t _pIdleThread;

   float *lv2Left [NUM_MIDI_PARTS + 1];
   float *lv2Right [NUM_MIDI_PARTS + 1];

   void process(uint32_t sample_count);
   void processMidiMessage(const uint8_t *msg);
   void *idleThread(void);
   std::vector <LV2_Program_Descriptor> flatbankprgs;
   const LV2_Descriptor *_lv2_desc;
public:
   YoshimiLV2Plugin(SynthEngine *synth, double sampleRate, const char *bundlePath, const LV2_Feature *const *features, const LV2_Descriptor *desc);
   virtual ~YoshimiLV2Plugin();
   bool init();

   //virtual methods from MusicIO
   unsigned int getSamplerate(void) {return _sampleRate; }
   int getBuffersize(void) {return _bufferSize; }
   bool Start(void) { return true; }
   void Close(void){;}

   bool openAudio() { return true; }
   bool openMidi() { return true; }

   virtual std::string audioClientName(void) { return "LV2 plugin"; }
   virtual int audioClientId(void) { return 0; }
   virtual std::string midiClientName(void) { return "LV2 plugin"; }
   virtual int midiClientId(void) { return 0; }

   virtual void registerAudioPort(int) {}

   //static methods
   static LV2_Handle	instantiate (const LV2_Descriptor *, double sample_rate, const char *bundle_path, const LV2_Feature *const *features);
   static void connect_port(LV2_Handle instance, uint32_t port, void *data_location);
   static void activate(LV2_Handle instance);
   static void deactivate(LV2_Handle instance);
   static void run(LV2_Handle instance, uint32_t   sample_count);
   static void cleanup(LV2_Handle instance);
   static const void * extension_data(const char * uri);

   LV2_State_Status stateSave(LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature *const * features);
   LV2_State_Status stateRestore(LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature *const * features);

   const LV2_Program_Descriptor * getProgram(uint32_t index);
   void selectProgramNew(unsigned char channel, uint32_t bank, uint32_t program);

   static void *static_idleThread(void *arg);

   static LV2_State_Status static_StateSave(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature *const * features);
   static LV2_State_Status static_StateRestore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature *const * features);

   static const LV2_Program_Descriptor * static_GetProgram(LV2_Handle handle, uint32_t index);
   static void static_SelectProgramNew(LV2_Handle handle, unsigned char channel, uint32_t bank, uint32_t program);
   static void static_SelectProgram(LV2_Handle handle, uint32_t bank, uint32_t program)
   {
       static_SelectProgramNew(handle, 0, bank, program);
   }

   /*
   static LV2_Worker_Status lv2wrk_work(LV2_Handle instance, LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle, uint32_t size, const void *data);
   static LV2_Worker_Status lv2wrk_response(LV2_Handle instance, uint32_t size, const void *body);
   static LV2_Worker_Status	lv2_wrk_end_run(LV2_Handle instance);
   */
   friend class YoshimiLV2PluginUI;

};

class YoshimiLV2PluginUI: public LV2_External_UI_Widget
{
private:
    YoshimiLV2Plugin *_plugin;
    LV2_External_UI_Host uiHost;
    MasterUI *_masterUI;
    LV2UI_Controller _controller;
    struct _externalUI
    {
        LV2_External_UI_Widget uiWIdget;
        YoshimiLV2PluginUI *uiInst;
    };
    _externalUI externalUI;
    LV2UI_Write_Function _write_function;
public:
    YoshimiLV2PluginUI(const char *, LV2UI_Write_Function, LV2UI_Controller, LV2UI_Widget *widget, const LV2_Feature *const *features);
    ~YoshimiLV2PluginUI();
    bool init();
    static LV2UI_Handle	instantiate(const LV2UI_Descriptor *descriptor, const char *plugin_uri, const char *bundle_path, LV2UI_Write_Function write_function, LV2UI_Controller controller, LV2UI_Widget *widget, const LV2_Feature *const *features);
    static void cleanup(LV2UI_Handle ui);
    static void static_guiClosed(void *arg);
    void run();
    void show();
    void hide();
    static void static_Run(struct _LV2_External_UI_Widget * _this_);
    static void static_Show(struct _LV2_External_UI_Widget * _this_);
    static void static_Hide(struct _LV2_External_UI_Widget * _this_);
};

#endif
