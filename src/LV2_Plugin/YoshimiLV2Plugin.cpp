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

#include "YoshimiLV2Plugin.h"
#include "Misc/Config.h"
#include "Misc/ConfBuild.h"
#include "Misc/SynthEngine.h"
#include "Interface/InterChange.h"
#include "Interface/Data2Text.h"
#include "Interface/Text2Data.h"
#include "Interface/MidiDecode.h"
#include "MusicIO/MusicClient.h"
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

#include <memory>
#include <string>
#include <thread>

using std::string;
using std::make_unique;

#define YOSHIMI_STATE_URI "http://yoshimi.sourceforge.net/lv2_plugin#state"

#define YOSHIMI_LV2_BUF_SIZE_URI    "http://lv2plug.in/ns/ext/buf-size"
#define YOSHIMI_LV2_BUF_SIZE_PREFIX YOSHIMI_LV2_BUF_SIZE_URI "#"

#define YOSHIMI_LV2_BUF_SIZE__maxBlockLength      YOSHIMI_LV2_BUF_SIZE_PREFIX "maxBlockLength"
#define YOSHIMI_LV2_BUF_SIZE__minBlockLength      YOSHIMI_LV2_BUF_SIZE_PREFIX "minBlockLength"
#define YOSHIMI_LV2_BUF_SIZE__nominalBlockLength      YOSHIMI_LV2_BUF_SIZE_PREFIX "nominalBlockLength"

#define YOSHIMI_LV2_OPTIONS_URI    "http://lv2plug.in/ns/ext/options"
#define YOSHIMI_LV2_OPTIONS_PREFIX YOSHIMI_LV2_OPTIONS_URI "#"

#define YOSHIMI_LV2_OPTIONS__Option          YOSHIMI_LV2_OPTIONS_PREFIX "Option"
#define YOSHIMI_LV2_OPTIONS__options         YOSHIMI_LV2_OPTIONS_PREFIX "options"

#define YOSHIMI_LV2_STATE__StateChanged      "http://lv2plug.in/ns/ext/state#StateChanged"



typedef enum {
    LV2_OPTIONS_INSTANCE,
    LV2_OPTIONS_RESOURCE,
    LV2_OPTIONS_BLANK,
    LV2_OPTIONS_PORT
} Yoshimi_LV2_Options_Context;


typedef struct _Yoshimi_LV2_Options_Option {
 Yoshimi_LV2_Options_Context context;  ///< Context (type of subject).
                    uint32_t subject;  ///< Subject.
                    LV2_URID key;      ///< Key (property).
                    uint32_t size;     ///< Size of value in bytes.
                    LV2_URID type;     ///< Type of value (datatype).
                 const void* value;    ///< Pointer to value (object).
} Yoshimi_LV2_Options_Option;




LV2_Descriptor yoshimi_lv2_desc =
{
    "http://yoshimi.sourceforge.net/lv2_plugin",
    YoshimiLV2Plugin::instantiate,
    YoshimiLV2Plugin::connect_port,
    YoshimiLV2Plugin::activate,
    YoshimiLV2Plugin::run,
    YoshimiLV2Plugin::deactivate,
    YoshimiLV2Plugin::cleanup,
    YoshimiLV2Plugin::extension_data
};


LV2_Descriptor yoshimi_lv2_multi_desc =
{
    "http://yoshimi.sourceforge.net/lv2_plugin_multi",
    YoshimiLV2Plugin::instantiate,
    YoshimiLV2Plugin::connect_port,
    YoshimiLV2Plugin::activate,
    YoshimiLV2Plugin::run,
    YoshimiLV2Plugin::deactivate,
    YoshimiLV2Plugin::cleanup,
    YoshimiLV2Plugin::extension_data
};

namespace {
    inline bool isMultiFeed(LV2_Descriptor const& desc)
    {
        return string{desc.URI} == string{yoshimi_lv2_multi_desc.URI};
    }
}


void YoshimiLV2Plugin::process(uint32_t sample_count)
{
    if (sample_count == 0) return;  // explicitly allowed by LV2 standard

    /*
     * Our implementation of LV2 has a problem with envelopes. In general
     * the bigger the buffer size the shorter the envelope, and whichever
     * is the smallest (host size or Yoshimi size) determines the time.
     *
     * However, Yoshimi is always correct when working standalone.
     */

    uint32_t processed = 0;
    BeatTracker::BeatValues beats(beatTracker->getRawBeatValues());
    uint32_t beatsAt = 0;
    bool bpmProvided = false;
    float *tmpLeft [NUM_MIDI_PARTS + 1];
    float *tmpRight [NUM_MIDI_PARTS + 1];
    MidiEvent intMidiEvent;
    for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
    {
        tmpLeft [i] = lv2Left [i];
        if (tmpLeft [i] == NULL)
            tmpLeft [i] = zynLeft [i];
        tmpRight [i] = lv2Right [i];
        if (tmpRight [i] == NULL)
            tmpRight [i] = zynRight [i];
    }
    LV2_ATOM_SEQUENCE_FOREACH(_midiDataPort, event)
    {
        if (event == NULL)
            continue;

        uint32_t next_frame = event->time.frames;
        if (next_frame >= sample_count)
            continue;

        // Avoid sample perfect alignment when not free wheeling (running
        // offline, as when rendering a track), because it is extremely
        // expensive when there are many MIDI events with just small timing
        // differences. It is also not real time safe, because the amount of
        // processing depends on the timing of the notes, not only by the number
        // of notes. Let the user control the granularity using buffer size
        // instead.
        uint32_t frameAlignment;
        if (isFreeWheel())
            frameAlignment = 1;
        else
            frameAlignment = synth.buffersize;
        while (next_frame - processed >= frameAlignment)
        {
            float bpmInc = (float)(processed - beatsAt) * beats.bpm / (synth.samplerate_f * 60.f);
            synth.setBeatValues(beats.songBeat + bpmInc, beats.monotonicBeat + bpmInc, beats.bpm);
            int mastered_chunk = synth.MasterAudio(tmpLeft, tmpRight, next_frame - processed);
            for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
            {
                tmpLeft [i] += mastered_chunk;
                tmpRight [i] += mastered_chunk;
            }
            processed += mastered_chunk;
        }

        if (event->body.type == _midi_event_id)
        {
            if (event->body.size > sizeof(intMidiEvent.data))
                continue;

            //process this midi event
            const uint8_t *msg = (const uint8_t*)(event + 1);
            if (param_freeWheel)
                processMidiMessage(msg);
        }
        else if (event->body.type == _atom_blank || event->body.type == _atom_object)
        {
            LV2_Atom_Object *obj = (LV2_Atom_Object *)&event->body;
            if (obj->body.otype != _atom_position)
                continue;

            LV2_Atom *bpb = NULL;
            LV2_Atom *bar = NULL;
            LV2_Atom *barBeat = NULL;
            LV2_Atom *bpm = NULL;
            LV2_Atom *beatUnit = NULL;
            lv2_atom_object_get(obj,
                                _atom_bpb, &bpb,
                                _atom_bar, &bar,
                                _atom_bar_beat, &barBeat,
                                _atom_bpm, &bpm,
                                _atom_beatUnit, &beatUnit,
                                NULL);

            if (bpm && bpm->type == _atom_float)
            {
                beats.bpm = ((LV2_Atom_Float *)bpm)->body;
                bpmProvided = true;
                if (beatUnit && beatUnit->type == _atom_int)
                {
                    // In DAWs, Beats Per Minute really mean Quarter Beats Per
                    // Minute. Therefore we need to divide by four first, to
                    // get a whole beat, and then multiply that according to
                    // the time signature denominator. See this link for some
                    // background: https://music.stackexchange.com/a/109743
                    beats.bpm = beats.bpm / 4 * ((LV2_Atom_Int *)beatUnit)->body;
                }
            }

            uint32_t frame = event->time.frames;
            float bpmInc = (float)(frame - processed) * beats.bpm / (synth.samplerate_f * 60.f);

            if (bpb && bpb->type == _atom_float
                && bar && bar->type == _atom_long
                && barBeat && barBeat->type == _atom_float)
            {
                // There is a global beat number in the LV2 time spec, called
                // "beat", but Carla doesn't seem to deliver this correctly, so
                // piece it together from bar and barBeat instead.
                float lv2Bpb = ((LV2_Atom_Float *)bpb)->body;
                float lv2Bar = ((LV2_Atom_Long *)bar)->body;
                float lv2BarBeat = ((LV2_Atom_Float *)barBeat)->body;
                beats.songBeat = lv2Bar * lv2Bpb + lv2BarBeat;
            }
            else
                beats.songBeat += bpmInc;
            beats.monotonicBeat += bpmInc;
            beatsAt = frame;
        }
    }

    while (processed < sample_count)
    {
        float bpmInc = (float)(processed - beatsAt) * beats.bpm / (synth.samplerate_f * 60.f);
        synth.setBeatValues(beats.songBeat + bpmInc, beats.monotonicBeat + bpmInc, beats.bpm);
        int mastered_chunk = synth.MasterAudio(tmpLeft, tmpRight, sample_count - processed);
        for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
        {
            tmpLeft [i] += mastered_chunk;
            tmpRight [i] += mastered_chunk;
        }
        processed += mastered_chunk;
    }

    float bpmInc = (float)(sample_count - beatsAt) * beats.bpm / (synth.samplerate_f * 60.f);
    beats.songBeat += bpmInc;
    beats.monotonicBeat += bpmInc;
    if (!bpmProvided && lastFallbackBpm != synth.PbpmFallback)
        beats.bpm = synth.PbpmFallback;
    lastFallbackBpm = synth.PbpmFallback;
    beatTracker->setBeatValues(beats);

    LV2_Atom_Sequence *aSeq = static_cast<LV2_Atom_Sequence *>(_notifyDataPortOut);
    size_t neededAtomSize = sizeof(LV2_Atom_Event) + sizeof(LV2_Atom_Object_Body);
    size_t paddedSize = (neededAtomSize + 7U) & (~7U);
    if (synth.getNeedsSaving() && _notifyDataPortOut && aSeq->atom.size >= paddedSize) //notify host about plugin's changes
    {
        synth.setNeedsSaving(false);
        aSeq->atom.type = _atom_type_sequence;
        aSeq->atom.size = sizeof(LV2_Atom_Sequence_Body);
        aSeq->body.unit = 0;
        aSeq->body.pad = 0;
        LV2_Atom_Event *ev = reinterpret_cast<LV2_Atom_Event *>(aSeq + 1);
        ev->time.frames = 0;
        LV2_Atom_Object *aObj = reinterpret_cast<LV2_Atom_Object *>(&ev->body);
        aObj->atom.type = _atom_object;
        aObj->atom.size = sizeof(LV2_Atom_Object_Body);
        aObj->body.id = 0;
        aObj->body.otype =_atom_state_changed;

        aSeq->atom.size += paddedSize;
    }
    else if (aSeq)
    {
        aSeq->atom.size = sizeof(LV2_Atom_Sequence_Body);
    }

}


void YoshimiLV2Plugin::processMidiMessage(const uint8_t * msg)
{
    bool in_place = isFreeWheel();
    handleMidi(msg[0], msg[1], msg[2], in_place);
}



YoshimiLV2Plugin::YoshimiLV2Plugin(SynthEngine& _synth
                                  ,double sampleRate
                                  ,const char* bundlePath
                                  ,LV2_Feature const *const *features
                                  ,LV2_Descriptor const& lv2Desc
                                  )
    : MusicIO(_synth, make_unique<SinglethreadedBeatTracker>())
    , _sampleRate{static_cast<uint32_t>(sampleRate)}
    , _bufferSize{0}
    , _bundlePath{bundlePath}
    , _midiDataPort{nullptr}
    , _notifyDataPortOut{nullptr}
    , _midi_event_id{0}
    , _bufferPos{0}
    , _offsetPos{0}
    , param_freeWheel{nullptr}
    , flatbankprgs{}
    , lastFallbackBpm{-1}
    , isReady{false}
{
    _uridMap.handle = NULL;
    _uridMap.map = NULL;
    const LV2_Feature *f = NULL;
    const Yoshimi_LV2_Options_Option *options = NULL;
    while ((f = *features) != NULL)
    {
        if (strcmp(f->URI, LV2_URID__map) == 0)
        {
            _uridMap = *(static_cast<LV2_URID_Map *>(f->data));
        }
        else if (strcmp(f->URI, YOSHIMI_LV2_OPTIONS__options) == 0)
        {
            options = static_cast<Yoshimi_LV2_Options_Option *>(f->data);
        }
        ++features;
    }

    uint32_t nomBufSize = 0;
    if (_uridMap.map and options)
    {
        _midi_event_id = _uridMap.map(_uridMap.handle, LV2_MIDI__MidiEvent);
        _yoshimi_state_id = _uridMap.map(_uridMap.handle, YOSHIMI_STATE_URI);
        _atom_string_id = _uridMap.map(_uridMap.handle, LV2_ATOM__String);
        LV2_URID maxBufSz = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_BUF_SIZE__maxBlockLength);
        LV2_URID minBufSz = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_BUF_SIZE__minBlockLength);
        LV2_URID nomBufSz = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_BUF_SIZE__nominalBlockLength);
        _atom_int = _uridMap.map(_uridMap.handle, LV2_ATOM__Int);
        _atom_long = _uridMap.map(_uridMap.handle, LV2_ATOM__Long);
        _atom_float = _uridMap.map(_uridMap.handle, LV2_ATOM__Float);
        _atom_type_chunk = _uridMap.map(_uridMap.handle, LV2_ATOM__Chunk);
        _atom_type_sequence = _uridMap.map(_uridMap.handle, LV2_ATOM__Sequence);
        _atom_state_changed = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_STATE__StateChanged);
        _atom_object = _uridMap.map(_uridMap.handle, LV2_ATOM__Object);
        _atom_blank = _uridMap.map(_uridMap.handle, LV2_ATOM__Blank);
        _atom_event_transfer = _uridMap.map(_uridMap.handle, LV2_ATOM__eventTransfer);
        _atom_position = _uridMap.map(_uridMap.handle, LV2_TIME__Position);
        _atom_bpb = _uridMap.map(_uridMap.handle, LV2_TIME__beatsPerBar);
        _atom_bar = _uridMap.map(_uridMap.handle, LV2_TIME__bar);
        _atom_bar_beat = _uridMap.map(_uridMap.handle, LV2_TIME__barBeat);
        _atom_bpm = _uridMap.map(_uridMap.handle, LV2_TIME__beatsPerMinute);
        _atom_beatUnit = _uridMap.map(_uridMap.handle, LV2_TIME__beatUnit);
        while (options->size > 0 && options->value != NULL)
        {
            if (options->context == LV2_OPTIONS_INSTANCE)
            {
                if ((options->key == minBufSz || options->key == maxBufSz) && options->type == _atom_int)
                {
                    uint32_t bufSz = *static_cast<const uint32_t *>(options->value);
                    if (_bufferSize < bufSz)
                        _bufferSize = bufSz;
                }
                if (options->key == nomBufSz && options->type == _atom_int)
                    nomBufSize = *static_cast<const uint32_t *>(options->value);
            }
            ++options;
        }
    }

    //runtime().Log("Buffer size " + to_string(nomBufSize));
    if (nomBufSize > 0)
        _bufferSize = nomBufSize;
    else if (_bufferSize == 0)
        _bufferSize = MAX_BUFFER_SIZE;

    // Configuration for LV2 mode...
    runtime().isLV2 = true;
    runtime().isMultiFeed = isMultiFeed(lv2Desc);
    synth.setBPMAccurate(true);
}



/** create a new distinct Yoshimi plugin instance; `activate()` will be called prior to `run()`. */
LV2_Handle YoshimiLV2Plugin::instantiate(LV2_Descriptor const* desc, double sample_rate, const char *bundle_path, LV2_Feature const *const *features)
{
    YoshimiLV2Plugin* instance;
    auto instantiatePlugin = [&](SynthEngine& synth) -> MusicIO*
                                {
                                    instance = new YoshimiLV2Plugin(synth, sample_rate, bundle_path, features, *desc);
                                    return instance;  // note: will be stored/managed in MusicClient
                                };

    if (Config::instances().startPluginInstance(instantiatePlugin))
    {
        assert(instance);
        instance->isReady.store(true, std::memory_order_release); // after this point, GUI-plugin may attach
        return static_cast<LV2_Handle>(instance);
    }
    else
        return nullptr;
}

/** Initialise the plugin instance and activate it for use. */
void YoshimiLV2Plugin::activate(LV2_Handle h)
{
    self(h).runtime().Log("Yoshimi LV2 plugin activated");
}

void YoshimiLV2Plugin::run(LV2_Handle h, uint32_t sample_count)
{
    self(h).process(sample_count);
}

void YoshimiLV2Plugin::deactivate(LV2_Handle h)
{
    self(h).runtime().Log("Yoshimi LV2 plugin deactivated");
}

/** called by LV2 host to destroy a plugin instance */
void YoshimiLV2Plugin::cleanup(LV2_Handle h)
{
    auto synthID = self(h).synth.getUniqueId();
    Config::instances().terminatePluginInstance(synthID);
}



bool YoshimiLV2Plugin::openAudio()
{
    bool validSettings = not (_uridMap.map == NULL
                             or _sampleRate == 0
                             or _bufferSize == 0
                             or _midi_event_id == 0
                             or _yoshimi_state_id == 0
                             or _atom_string_id == 0);
    return validSettings
       and prepBuffers();
}

bool YoshimiLV2Plugin::openMidi()
{
    return true; /*nothing to do*/
}

bool YoshimiLV2Plugin::Start()
{
    // by default do not launch UI; rather create it later, on-demand
    runtime().showGui = false;
    memset(lv2Left, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(lv2Right, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));

    return true;
}


void YoshimiLV2Plugin::connect_port(LV2_Handle handle, uint32_t port, void* data_location)
{
    if (port > NUM_MIDI_PARTS + 2)
        return;
    YoshimiLV2Plugin& plugin = self(handle);
     if (port == 0)//atom midi event port
     {
         plugin._midiDataPort = static_cast<LV2_Atom_Sequence*>(data_location);
         return;
     }
     else if (port == 1) //freewheel control port
     {
         plugin.param_freeWheel = static_cast<float*>(data_location);
         return;
     }
     else if (port == 36 and plugin.runtime().isMultiFeed) //notify out port
     {
         plugin._notifyDataPortOut = static_cast<LV2_Atom_Sequence*>(data_location);
         return;
     }
     else if (port == 4 and not plugin.runtime().isMultiFeed) //notify out port
     {
         plugin._notifyDataPortOut = static_cast<LV2_Atom_Sequence*>(data_location);
         return;
     }

     port -=2;

     if (port == 0) //main outl
         port = NUM_MIDI_PARTS * 2;
     else if (port == 1) //main outr
         port = NUM_MIDI_PARTS * 2 + 1;
     else
         port -= 2;

     int portIndex = static_cast<int>(floorf((float)port/2.0f));
     if (port % 2 == 0) //left channel
         plugin.lv2Left[portIndex] = static_cast<float *>(data_location);
     else
         plugin.lv2Right[portIndex] = static_cast<float *>(data_location);
}



LV2_Programs_Interface yoshimi_prg_iface =
{
    YoshimiLV2Plugin::callback_getProgram,
    YoshimiLV2Plugin::callback_selectProgram,
    YoshimiLV2Plugin::callback_selectProgramNew
};


const void *YoshimiLV2Plugin::extension_data(const char *uri)
{
    static const LV2_State_Interface state_iface = { YoshimiLV2Plugin::callback_stateSave, YoshimiLV2Plugin::callback_stateRestore };
    if (!strcmp(uri, LV2_STATE__interface))
    {
        return static_cast<const void *>(&state_iface);

    }
    else if (strcmp(uri, LV2_PROGRAMSNEW__Interface) == 0)
    {
        return static_cast<const void *>(&yoshimi_prg_iface);
    }
    else if (strcmp(uri, LV2_PROGRAMS__Interface) == 0)
    {
        return static_cast<const void *>(&yoshimi_prg_iface);
    }

    return NULL;
}


LV2_State_Status YoshimiLV2Plugin::stateSave(LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature * const *features)
{
    uint32_t a = flags; flags = a;
    const LV2_Feature * const *feat = features;
    features = feat;
    // suppress warnings - may use later

    char *data = NULL;
    int sz = synth.getalldata(&data);

    store(handle, _yoshimi_state_id, data, sz, _atom_string_id, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    free(data);
    return LV2_STATE_SUCCESS;
}


LV2_State_Status YoshimiLV2Plugin::stateRestore(LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature * const *features)
{
    uint32_t a = flags; flags = a;
    const LV2_Feature * const *feat = features;
    features = feat;
    // lines above suppress warnings - may use later

    size_t sz = 0;
    LV2_URID type = 0;
    uint32_t new_flags;

    const char *data = (const char *)retrieve(handle, _yoshimi_state_id, &sz, &type, &new_flags);

    if (sz > 0)
        synth.putalldata(data, sz);
    return LV2_STATE_SUCCESS;
}


LV2_Program_Descriptor const * YoshimiLV2Plugin::getProgram(uint32_t index)
{
    if (flatbankprgs.empty())
    {
        BankEntryMap const& banks{synth.bank.getBanks(runtime().currentRoot)};
        for (auto& [bankID,bank] : banks)
            if (not bank.dirname.empty())
                for (auto& [instrumentID,instrument] : bank.instruments)
                    if (not instrument.name.empty())
                    {
                        LV2Bank entry;
                        entry.bank    = bankID;
                        entry.program = instrumentID;
                        entry.display = bank.dirname + " -> " + instrument.name;
                        entry.name    = entry.display.c_str();
                        flatbankprgs.push_back(entry);
                    }
    }
    return index < flatbankprgs.size()? &flatbankprgs [index]
                                      : nullptr;
}


void YoshimiLV2Plugin::selectProgramNew(unsigned char channel, uint32_t bank, uint32_t program)
{
    if (runtime().midi_bank_C != 128)
    {
        synth.mididecode.setMidiBankOrRootDir((short)bank, isFreeWheel());
    }
    synth.mididecode.setMidiProgram(channel, program, isFreeWheel());
}


LV2_State_Status YoshimiLV2Plugin::callback_stateSave(LV2_Handle h, LV2_State_Store_Function store, LV2_State_Handle state, uint32_t flags, const LV2_Feature * const *features)
{
    return self(h).stateSave(store, state, flags, features);
}


LV2_State_Status YoshimiLV2Plugin::callback_stateRestore(LV2_Handle h, LV2_State_Retrieve_Function retrieve, LV2_State_Handle state, uint32_t flags, const LV2_Feature * const *features)
{
    return self(h).stateRestore(retrieve, state, flags, features);
}


const LV2_Program_Descriptor *YoshimiLV2Plugin::callback_getProgram(LV2_Handle h, uint32_t index)
{
    return self(h).getProgram(index);
}


void YoshimiLV2Plugin::callback_selectProgramNew(LV2_Handle h, unsigned char channel, uint32_t bank, uint32_t program)
{
    return self(h).selectProgramNew(channel, bank, program);
}




YoshimiLV2PluginUI::YoshimiLV2PluginUI(const char *, LV2UI_Write_Function, LV2UI_Controller controller,
                                       LV2UI_Widget* widget, LV2_Feature const *const *features)
    : corePlugin(nullptr)
    , plugin_human_id{"Yoshimi lv2 plugin"}
    , notify_on_GUI_close{}
{
    // Configure callbacks for running the UI
    LV2_External_UI_Widget::run  = YoshimiLV2PluginUI::callback_Run;
    LV2_External_UI_Widget::show = YoshimiLV2PluginUI::callback_Show;
    LV2_External_UI_Widget::hide = YoshimiLV2PluginUI::callback_Hide;

    while (*features)
    {
        LV2_Feature const* f = *features;
        if (strcmp(f->URI, LV2_INSTANCE_ACCESS_URI) == 0)
        {
            corePlugin = static_cast<YoshimiLV2Plugin *>(f->data);
        }
        else if (strcmp(f->URI, LV2_EXTERNAL_UI__Host) == 0)
        {
            LV2_External_UI_Host& hostSpec = * static_cast<LV2_External_UI_Host *>(f->data);
            plugin_human_id = hostSpec.plugin_human_id;
            auto callback = hostSpec.ui_closed;
            notify_on_GUI_close = [callback,controller]{ callback(controller); };
        }
        ++features;
    }
    // this object also serves as »widget« for the event callbacks
    *widget = static_cast<LV2UI_Widget>(this);
}

YoshimiLV2PluginUI::~YoshimiLV2PluginUI()
{
    engine().shutdownGui();
    Fl::check(); // necessary to ensure screen redraw after all windows are hidden
}


bool YoshimiLV2PluginUI::init()
{
    if (not (corePlugin and notify_on_GUI_close))
        return false;

    // LV2 hosts may load plugins concurrently, which in some corner cases
    // causes a race between SynthEngine initialisation and bootstrap of the GUI
    while (not corePlugin->isReady.load(std::memory_order_acquire))
        std::this_thread::yield();

    engine().installGuiClosedCallback([this]
                                        {// invoked by SynthEngine when FLTK GUI is closed explicitly...
                                            engine().shutdownGui();
                                            notify_on_GUI_close();
                                        });
    return true;
}


/** activated by LV2 host when preparing to launch the GUI: create a UI plugin associated with a core plugin instance */
LV2UI_Handle YoshimiLV2PluginUI::instantiate(LV2UI_Descriptor const*, const char* /*plugin_uri*/, const char* bundle_path,
                                             LV2UI_Write_Function write_function, LV2UI_Controller controller,
                                             LV2UI_Widget* widget, const LV2_Feature * const *features)
{
    YoshimiLV2PluginUI* uiinst = new YoshimiLV2PluginUI(bundle_path, write_function, controller, widget, features);
    if (uiinst->init())
    {
        return static_cast<LV2UI_Handle>(uiinst);
    }
    else
        delete uiinst;
    return NULL;
}

/** called by LV2 host to discard the plugin GUI;
 *  alternatively the host may choose just to hide the UI */
void YoshimiLV2PluginUI::cleanup(LV2UI_Handle ui)
{
    YoshimiLV2PluginUI *uiinst = static_cast<YoshimiLV2PluginUI *>(ui);
    delete uiinst;
}

/** recurring GUI event handling cycle*/
void YoshimiLV2PluginUI::run()
{
    if (isGuiActive())
    {
        masterUI().checkBuffer();
        Fl::check();
    }
    else
        notify_on_GUI_close();
}


void YoshimiLV2PluginUI::show()
{
    if (not isGuiActive())
    {
        initFltkLock();
        Config::instances().launchGui_forPlugin(engine().getUniqueId(), plugin_human_id);
    }
    else
        masterUI().masterwindow->show();
}


void YoshimiLV2PluginUI::hide()
{
    if (isGuiActive())
        masterUI().masterwindow->hide();
}


/**
 * This function was introduced as an attempt to be defensive and handle FLTK locking properly.
 * We use the LV2 extension "http://yoshimi.sourceforge.net/lv2_plugin#ExternalUI"
 * and I could not find any documentation to rule out that some host may invoke the
 * LV2_External_UI_Widget run() function concurrently for two distinct GUI instances.
 * On the other hand, FLTK by default runs single threaded (as most UI toolkits to)
 * and has clearly stated rules how to deal with concurrency.
 * See https://www.fltk.org/doc-1.3/advanced.html
 * This function ensures thus that the Fl::lock() is set _once and initially_,
 * before creating the first window.
 */
void YoshimiLV2PluginUI::initFltkLock()
{
    static bool firstTime{true};
    if (firstTime)
    {
        Fl::lock();
        firstTime = false;
    }
}




/** Entry point for the Host to discover and load the core plugin */
extern "C" const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    switch (index)
    {
    case 0:
        return &yoshimi_lv2_desc;
    case 1:
        return &yoshimi_lv2_multi_desc;
    default:
        break;
    }
    return NULL;
}


LV2UI_Descriptor yoshimi_lv2ui_desc =
{
    "http://yoshimi.sourceforge.net/lv2_plugin#ExternalUI",
    YoshimiLV2PluginUI::instantiate,
    YoshimiLV2PluginUI::cleanup,
    NULL,
    NULL
};


/** Entry point for the Host to discover and load the associated GUI plugin */
extern "C" const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
    switch (index)
    {
    case 0:
        return &yoshimi_lv2ui_desc;
    default:
        break;
    }
    return NULL;

}
