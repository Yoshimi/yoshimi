/*
    YoshimiLV2Plugin

    Copyright 2014, Andrew Deryabin <andrewderyabin@gmail.com>
    Copyright 2016-2024, Will Godfrey, Kristian Amlie, Ichthyostega and others.

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
#include <functional>
#include <atomic>
#include <string>
#include <vector>

#include "Misc/SynthEngine.h"
#include "Interface/InterChange.h"
#include "Interface/Data2Text.h"
#include "Interface/RingBuffer.h"
#include "MusicIO/MusicIO.h"

using std::string;


class YoshimiLV2Plugin : public MusicIO
{
private:
   uint32_t _sampleRate;
   uint32_t _bufferSize;
   string _bundlePath;
   LV2_URID_Map _uridMap;
   LV2_Atom_Sequence *_midiDataPort;
   LV2_Atom_Sequence *_notifyDataPortOut;
   LV2_URID _midi_event_id;
   LV2_URID _yoshimi_state_id;
   LV2_URID _atom_string_id;
   LV2_URID _atom_int;
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
   LV2_URID _atom_beatUnit;
   uint32_t _bufferPos;
   uint32_t _offsetPos;

   float* param_freeWheel;
   inline bool isFreeWheel() const { return param_freeWheel and (*param_freeWheel != 0); }

    struct MidiEvent {
        uint32_t time;
        char data[4]; // all events of interest are <= 4bytes
    };

    struct LV2Bank : LV2_Program_Descriptor{
        string display;
    };
    std::vector<LV2Bank> flatbankprgs;

    float lastFallbackBpm;
    std::atomic_bool isReady;

    float *lv2Left [NUM_MIDI_PARTS + 1];
    float *lv2Right [NUM_MIDI_PARTS + 1];


    static YoshimiLV2Plugin& self(LV2_Handle handle) { assert(handle); return * static_cast<YoshimiLV2Plugin *>(handle); }

public:
    // shall not be copied nor moved
    YoshimiLV2Plugin(YoshimiLV2Plugin&&)                 = delete;
    YoshimiLV2Plugin(YoshimiLV2Plugin const&)            = delete;
    YoshimiLV2Plugin& operator=(YoshimiLV2Plugin&&)      = delete;
    YoshimiLV2Plugin& operator=(YoshimiLV2Plugin const&) = delete;
    YoshimiLV2Plugin(SynthEngine&, double sampleRate, const char *bundlePath, LV2_Feature const *const *features, LV2_Descriptor const& desc);

    /* ====== MusicIO interface ======== */
    bool openAudio()               override ;
    bool openMidi()                override ;
    bool Start()                   override ;
    void Close()                   override { /*ignore*/ }
    void registerAudioPort(int)    override { /*ignore*/ }

    uint getSamplerate()     const override { return _sampleRate; }
    int getBuffersize()      const override { return _bufferSize; }
    string audioClientName() const override { return "LV2 plugin"; }
    int audioClientId()      const override { return 0; }
    string midiClientName()  const override { return "LV2 plugin"; }
    int midiClientId()       const override { return 0; }

   //static LV2 callback functions
   static LV2_Handle instantiate (const LV2_Descriptor*, double sample_rate, const char* bundle_path, LV2_Feature const* const* features);
   static void connect_port(LV2_Handle instance, uint32_t port, void *data_location);
   static void activate(LV2_Handle instance);
   static void deactivate(LV2_Handle instance);
   static void run(LV2_Handle instance, uint32_t sample_count);
   static void cleanup(LV2_Handle instance);
   static const void * extension_data(const char * uri);


   static LV2_State_Status callback_stateSave(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, LV2_Feature const* const* features);
   static LV2_State_Status callback_stateRestore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, LV2_Feature const* const* features);

   static const LV2_Program_Descriptor * callback_getProgram(LV2_Handle handle, uint32_t index);
   static void callback_selectProgramNew(LV2_Handle handle, unsigned char channel, uint32_t bank, uint32_t program);
   static void callback_selectProgram(LV2_Handle handle, uint32_t bank, uint32_t program)
   {
       callback_selectProgramNew(handle, 0, bank, program);
   }

private:
   void process(uint32_t sample_count);
   void processMidiMessage(const uint8_t* msg);
   LV2_State_Status stateSave(LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, LV2_Feature const* const* features);
   LV2_State_Status stateRestore(LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, LV2_Feature const* const* features);

   LV2_Program_Descriptor const* getProgram(uint32_t index);
   void selectProgramNew(uchar channel, uint32_t bank, uint32_t program);

   friend class YoshimiLV2PluginUI;
};


class YoshimiLV2PluginUI : public LV2_External_UI_Widget
{

    YoshimiLV2Plugin *corePlugin;
    string plugin_human_id;
    std::function<void()> notify_on_GUI_close;

    static YoshimiLV2PluginUI& self(void* handle) { assert(handle); return * static_cast<YoshimiLV2PluginUI *>(handle); }

public:
    YoshimiLV2PluginUI(const char*, LV2UI_Write_Function, LV2UI_Controller, LV2UI_Widget*, LV2_Feature const *const *);
    ~YoshimiLV2PluginUI();
    bool init();
    static LV2UI_Handle	instantiate(const LV2UI_Descriptor *descriptor, const char *plugin_uri, const char *bundle_path, LV2UI_Write_Function write_function, LV2UI_Controller controller, LV2UI_Widget *widget, const LV2_Feature *const *features);
    static void cleanup(LV2UI_Handle ui);
    void run();
    void show();
    void hide();
    static void callback_Run (LV2_External_UI_Widget* ui){ self(ui).run();  }
    static void callback_Show(LV2_External_UI_Widget* ui){ self(ui).show(); }
    static void callback_Hide(LV2_External_UI_Widget* ui){ self(ui).hide(); }

private:
    SynthEngine& engine() { return corePlugin->synth; } // use friend access
    MasterUI& masterUI()  { assert(isGuiActive()); return * engine().getGuiMaster(); }
    bool isGuiActive()    { return bool(engine().getGuiMaster()); }
    void initFltkLock();
};

#endif /*YOSHIMI_LV2_PLUGIN_H*/
