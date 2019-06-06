/*
    YoshimiLV2Plugin

    Copyright 2014, Andrew Deryabin <andrewderyabin@gmail.com>
    Copyright 2016-2019, Will Godfrey & others.

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

    Modified May 2019
*/

#include "YoshimiLV2Plugin.h"
#include "Misc/Config.h"
#include "Misc/ConfBuild.h"
#include "Misc/SynthEngine.h"
#include "Interface/InterChange.h"
#include "Interface/MidiDecode.h"
#include "MusicIO/MusicClient.h"
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define YOSHIMI_STATE_URI "http://yoshimi.sourceforge.net/lv2_plugin#state"

#define YOSHIMI_LV2_BUF_SIZE_URI    "http://lv2plug.in/ns/ext/buf-size"
#define YOSHIMI_LV2_BUF_SIZE_PREFIX YOSHIMI_LV2_BUF_SIZE_URI "#"

#define YOSHIMI_LV2_BUF_SIZE__maxBlockLength      YOSHIMI_LV2_BUF_SIZE_PREFIX "maxBlockLength"
#define YOSHIMI_LV2_BUF_SIZE__minBlockLength      YOSHIMI_LV2_BUF_SIZE_PREFIX "minBlockLength"

#define YOSHIMI_LV2_OPTIONS_URI    "http://lv2plug.in/ns/ext/options"
#define YOSHIMI_LV2_OPTIONS_PREFIX YOSHIMI_LV2_OPTIONS_URI "#"

#define YOSHIMI_LV2_OPTIONS__Option          YOSHIMI_LV2_OPTIONS_PREFIX "Option"
#define YOSHIMI_LV2_OPTIONS__options         YOSHIMI_LV2_OPTIONS_PREFIX "options"

#define YOSHIMI_LV2_STATE__StateChanged      "http://lv2plug.in/ns/ext/state#StateChanged"

extern SynthEngine *firstSynth;

typedef enum {
    LV2_OPTIONS_INSTANCE,
    LV2_OPTIONS_RESOURCE,
    LV2_OPTIONS_BLANK,
    LV2_OPTIONS_PORT
} Yoshimi_LV2_Options_Context;


typedef struct _Yoshimi_LV2_Options_Option {
 Yoshimi_LV2_Options_Context context;  /**< Context (type of subject). */
 uint32_t            subject;  /**< Subject. */
 LV2_URID            key;      /**< Key (property). */
 uint32_t            size;     /**< Size of value in bytes. */
 LV2_URID            type;     /**< Type of value (datatype). */
 const void*         value;    /**< Pointer to value (object). */
} Yoshimi_LV2_Options_Option;


using namespace std;



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


void YoshimiLV2Plugin::process(uint32_t sample_count)
{
    if (sample_count == 0)
        return;

    float *tmpLeft [NUM_MIDI_PARTS + 1];
    float *tmpRight [NUM_MIDI_PARTS + 1];
    struct midi_event intMidiEvent;

    for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
    {
        tmpLeft [i] = lv2Left [i];
        if (tmpLeft [i] == NULL)
            tmpLeft [i] = zynLeft [i];
        tmpRight [i] = lv2Right [i];
        if (tmpRight [i] == NULL)
            tmpRight [i] = zynRight [i];
    }
    uint32_t next_frame = 0;
    uint32_t to_process = sample_count;
    uint32_t processed = 0;
    LV2_ATOM_SEQUENCE_FOREACH(_midiDataPort, event)
    {
        if (event == NULL)
            continue;
        if (event->body.size >= sizeof(intMidiEvent.data))
            continue;
        if (event->body.type != _midi_event_id)
            continue;
        {
            next_frame = event->time.frames;
            //process this midi event

            if (next_frame >= sample_count)
                continue;
            while (processed < next_frame && to_process >= ActualBufferSize)
            {
                _synth->MasterAudio(tmpLeft, tmpRight, ActualBufferSize);
                processed += ActualBufferSize;
                to_process -= ActualBufferSize;
            }
            if (to_process > 0)
            {
                _synth->MasterAudio(tmpLeft, tmpRight, to_process);
                processed += to_process;
                to_process = 0;
            }
            const uint8_t *msg = (const uint8_t*)(event + 1);
            if (_bFreeWheel != NULL)
                processMidiMessage(msg);
        }
    }

    if (to_process > 0)
    {
        while (to_process >= ActualBufferSize)
        {
            _synth->MasterAudio(tmpLeft, tmpRight, ActualBufferSize);
            for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
            {
                tmpLeft [i] += ActualBufferSize;
                tmpRight [i] += ActualBufferSize;
            }
            processed += ActualBufferSize;
            to_process -= ActualBufferSize;
        }
        if (to_process > 0)
        {
            _synth->MasterAudio(tmpLeft, tmpRight, to_process);
            for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
            {
                tmpLeft [i] += to_process;
                tmpRight [i] += to_process;
            }
            processed += to_process;
            to_process = 0;
        }
    }
    for (uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
    {
        tmpLeft [i] += processed;
        tmpRight [i] += processed;
    }

    LV2_Atom_Sequence *aSeq = static_cast<LV2_Atom_Sequence *>(_notifyDataPortOut);
    size_t neededAtomSize = sizeof(LV2_Atom_Event) + sizeof(LV2_Atom_Object_Body);
    size_t paddedSize = (neededAtomSize + 7U) & (~7U);
    if(synth->getNeedsSaving() && _notifyDataPortOut && aSeq->atom.size >= paddedSize) //notify host about plugin's changes
    {
        synth->setNeedsSaving(false);
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
    else if(aSeq)
    {
        aSeq->atom.size = sizeof(LV2_Atom_Sequence_Body);
    }
}


void YoshimiLV2Plugin::processMidiMessage(const uint8_t * msg)
{
    bool in_place = _bFreeWheel ? ((*_bFreeWheel == 0) ? false : true) : false;
    setMidi(msg[0], msg[1], msg[2], in_place);
}


void *YoshimiLV2Plugin::idleThread()
{
    //temporary
//    _synth->getRuntime().showGui = true;
//    MasterUI *guiMaster = _synth->getGuiMaster();
//    if (guiMaster == NULL)
//    {
//        _synth->getRuntime().Log("Failed to instantiate gui");
//        return NULL;
//    }
//    guiMaster->Init("yoshimi lv2 plugin");

    while (_synth->getRuntime().runSynth)
    {
//        // where all the action is ...
//        if (_synth->getRuntime().showGui)
//            Fl::wait(0.033333);
//        else
            usleep(33333);

    }
    return NULL;
}


YoshimiLV2Plugin::YoshimiLV2Plugin(SynthEngine *synth, double sampleRate, const char *bundlePath, const LV2_Feature *const *features, const LV2_Descriptor *desc):
    MusicIO(synth),
    _synth(synth),
    _sampleRate(static_cast<uint32_t>(sampleRate)),
    _bufferSize(0),
    _bundlePath(bundlePath),
    _midiDataPort(NULL),
    _notifyDataPortOut(NULL),
    _midi_event_id(0),
    _bufferPos(0),
    _offsetPos(0),
    _bFreeWheel(NULL),
    _pIdleThread(0),
    _lv2_desc(desc)
{
    flatbankprgs.clear();
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

    if (_uridMap.map != NULL && options != NULL)
    {
        _midi_event_id = _uridMap.map(_uridMap.handle, LV2_MIDI__MidiEvent);
        _yoshimi_state_id = _uridMap.map(_uridMap.handle, YOSHIMI_STATE_URI);
        _atom_string_id = _uridMap.map(_uridMap.handle, LV2_ATOM__String);
        LV2_URID maxBufSz = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_BUF_SIZE__maxBlockLength);
        LV2_URID minBufSz = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_BUF_SIZE__minBlockLength);
        LV2_URID atomInt = _uridMap.map(_uridMap.handle, LV2_ATOM__Int);
        _atom_type_chunk = _uridMap.map(_uridMap.handle, LV2_ATOM__Chunk);
        _atom_type_sequence = _uridMap.map(_uridMap.handle, LV2_ATOM__Sequence);
        _atom_state_changed = _uridMap.map(_uridMap.handle, YOSHIMI_LV2_STATE__StateChanged);
        _atom_object = _uridMap.map(_uridMap.handle, LV2_ATOM__Object);
        _atom_event_transfer = _uridMap.map(_uridMap.handle, LV2_ATOM__eventTransfer);
        while (options->size > 0 && options->value != NULL)
        {
            if (options->context == LV2_OPTIONS_INSTANCE)
            {
                if ((options->key == minBufSz || options->key == maxBufSz) && options->type == atomInt)
                {
                    uint32_t bufSz = *static_cast<const uint32_t *>(options->value);
                    if (_bufferSize < bufSz)
                        _bufferSize = bufSz;
                }
            }
            ++options;
        }
    }

    if (_bufferSize == 0)
        _bufferSize = MAX_BUFFER_SIZE;
}


YoshimiLV2Plugin::~YoshimiLV2Plugin()
{
    if (_synth != NULL)
    {
        if (!flatbankprgs.empty())
        {
            getProgram(flatbankprgs.size() + 1);
        }
        _synth->getRuntime().runSynth = false;
        pthread_join(_pIdleThread, NULL);
        delete _synth;
        _synth = NULL;
    }
}


bool YoshimiLV2Plugin::init()
{
    if (_uridMap.map == NULL || _sampleRate == 0 || _bufferSize == 0 || _midi_event_id == 0 || _yoshimi_state_id == 0 || _atom_string_id == 0)
        return false;
    if (!prepBuffers())
        return false;

    if(!_synth->Init(_sampleRate))
    {
        synth->getRuntime().LogError("Can't init synth engine");
	return false;
    }
    if (_synth->getUniqueId() == 0)
    {
        firstSynth = _synth;
        //firstSynth->getRuntime().Log("Started first");
    }

    _synth->getRuntime().showGui = false;

    memset(lv2Left, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(lv2Right, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));

    _synth->getRuntime().runSynth = true;

    if (!_synth->getRuntime().startThread(&_pIdleThread, YoshimiLV2Plugin::static_idleThread, this, false, 0, "LV2 idle"))
    {
        synth->getRuntime().Log("Failed to start idle thread");
        return false;
    }

    synth->getRuntime().Log("Starting in LV2 plugin mode");
    return true;
}


LV2_Handle	YoshimiLV2Plugin::instantiate (const struct _LV2_Descriptor *desc, double sample_rate, const char *bundle_path, const LV2_Feature *const *features)
{
    SynthEngine *synth = new SynthEngine(0, NULL, true);
    if (synth == NULL)
        return NULL;
    YoshimiLV2Plugin *inst = new YoshimiLV2Plugin(synth, sample_rate, bundle_path, features, desc);
    if (inst->init())
        return static_cast<LV2_Handle>(inst);
    else {
        synth->getRuntime().LogError("Failed to create Yoshimi LV2 plugin");
        delete inst;
    }
    return NULL;
}


void YoshimiLV2Plugin::connect_port(LV2_Handle instance, uint32_t port, void *data_location)
{
    if (port > NUM_MIDI_PARTS + 2)
        return;
     YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
     if (port == 0)//atom midi event port
     {
         inst->_midiDataPort = static_cast<LV2_Atom_Sequence *>(data_location);
         return;
     }
     else if (port == 1) //freewheel control port
     {
         inst->_bFreeWheel = static_cast<float *>(data_location);
         return;
     }
     else if (port == 36 && std::string(inst->_lv2_desc->URI) == std::string(yoshimi_lv2_multi_desc.URI)) //notify out port
     {
         inst->_notifyDataPortOut = static_cast<LV2_Atom_Sequence *>(data_location);
         return;
     }
     else if (port == 4 && std::string(inst->_lv2_desc->URI) == std::string(yoshimi_lv2_desc.URI)) //notify out port
     {
         inst->_notifyDataPortOut = static_cast<LV2_Atom_Sequence *>(data_location);
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
         inst->lv2Left[portIndex] = static_cast<float *>(data_location);
     else
         inst->lv2Right[portIndex] = static_cast<float *>(data_location);
}


void YoshimiLV2Plugin::activate(LV2_Handle instance)
{
    YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
    inst->Start();
}


void YoshimiLV2Plugin::deactivate(LV2_Handle instance)
{
    YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
    inst->Close();
}


void YoshimiLV2Plugin::run(LV2_Handle instance, uint32_t sample_count)
{
    YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
    inst->process(sample_count);
}

void YoshimiLV2Plugin::cleanup(LV2_Handle instance)
{
    YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
    delete inst;
}



LV2_Programs_Interface yoshimi_prg_iface =
{
    YoshimiLV2Plugin::static_GetProgram,
    YoshimiLV2Plugin::static_SelectProgram,
    YoshimiLV2Plugin::static_SelectProgramNew
};


const void *YoshimiLV2Plugin::extension_data(const char *uri)
{
    static const LV2_State_Interface state_iface = { YoshimiLV2Plugin::static_StateSave, YoshimiLV2Plugin::static_StateRestore };
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
    int sz = _synth->getalldata(&data);

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
        _synth->putalldata(data, sz);
    return LV2_STATE_SUCCESS;
}


const LV2_Program_Descriptor *YoshimiLV2Plugin::getProgram(uint32_t index)
{
    if (flatbankprgs.empty())
    {
        Bank &bankObj = synth->getBankRef();
        const BankEntryMap &banks = bankObj.getBanks(synth->getRuntime().currentRoot);
        BankEntryMap::const_iterator itB;
        InstrumentEntryMap::const_iterator itI;
        for (itB = banks.begin(); itB != banks.end(); ++itB)
        {
            string bankName = itB->second.dirname;
            if (!bankName.empty())
            {
                for (itI = itB->second.instruments.begin(); itI != itB->second.instruments.end(); ++itI)
                {
                    if (!itI->second.name.empty())
                    {
                        LV2_Program_Descriptor desc;
                        desc.bank = itB->first;
                        desc.program = itI->first;
                        desc.name = strdup((bankName + " -> " + itI->second.name).c_str());
                        flatbankprgs.push_back(desc);
                    }
                }
            }
        }
    }

    if (index >= flatbankprgs.size())
    {
        for (size_t i = 0; i < flatbankprgs.size(); ++i)
        {
            if (flatbankprgs [i].name != NULL)
            {
                free(const_cast<char *>(flatbankprgs [i].name));
            }
        }
        flatbankprgs.clear();
        return NULL;
    }
    return &flatbankprgs [index];
}


void YoshimiLV2Plugin::selectProgramNew(unsigned char channel, uint32_t bank, uint32_t program)
{
    bool isFreeWheel = false;
    if (_bFreeWheel && *_bFreeWheel == 1)
        isFreeWheel = true;
    if (_synth->getRuntime().midi_bank_C != 128)
    {
        synth->mididecode.setMidiBankOrRootDir((short)bank, isFreeWheel);
    }
    synth->mididecode.setMidiProgram(channel, program, isFreeWheel);
}


void *YoshimiLV2Plugin::static_idleThread(void *arg)
{
    return static_cast<YoshimiLV2Plugin *>(arg)->idleThread();
}


LV2_State_Status YoshimiLV2Plugin::static_StateSave(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle, uint32_t flags, const LV2_Feature * const *features)
{
    return static_cast<YoshimiLV2Plugin *>(instance)->stateSave(store, handle, flags, features);
}


LV2_State_Status YoshimiLV2Plugin::static_StateRestore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature * const *features)
{
    return static_cast<YoshimiLV2Plugin *>(instance)->stateRestore(retrieve, handle, flags, features);
}


const LV2_Program_Descriptor *YoshimiLV2Plugin::static_GetProgram(LV2_Handle handle, uint32_t index)
{
    return static_cast<YoshimiLV2Plugin *>(handle)->getProgram(index);
}


void YoshimiLV2Plugin::static_SelectProgramNew(LV2_Handle handle, unsigned char channel, uint32_t bank, uint32_t program)
{
    return static_cast<YoshimiLV2Plugin *>(handle)->selectProgramNew(channel, bank, program);
}




YoshimiLV2PluginUI::YoshimiLV2PluginUI(const char *, LV2UI_Write_Function write_function, LV2UI_Controller controller, LV2UI_Widget *widget, const LV2_Feature * const *features)
    :_plugin(NULL),
     _masterUI(NULL),
     _controller(controller),
     _write_function(write_function)
{
    uiHost.plugin_human_id = NULL;
    uiHost.ui_closed = NULL;
    const LV2_Feature *f = NULL;
    externalUI.uiWIdget.run = YoshimiLV2PluginUI::static_Run;
    externalUI.uiWIdget.show = YoshimiLV2PluginUI::static_Show;
    externalUI.uiWIdget.hide = YoshimiLV2PluginUI::static_Hide;
    externalUI.uiInst = this;
    while ((f = *features) != NULL)
    {
        if (strcmp(f->URI, LV2_INSTANCE_ACCESS_URI) == 0)
        {
            _plugin = static_cast<YoshimiLV2Plugin *>(f->data);
        }
        else if (strcmp(f->URI, LV2_EXTERNAL_UI__Host) == 0)
        {
            uiHost.plugin_human_id = strdup(static_cast<LV2_External_UI_Host *>(f->data)->plugin_human_id);
            uiHost.ui_closed = static_cast<LV2_External_UI_Host *>(f->data)->ui_closed;
        }
        ++features;
    }
    if (uiHost.plugin_human_id == NULL)
    {
        uiHost.plugin_human_id = strdup("Yoshimi lv2 plugin");
    }
    *widget = &externalUI;
}

YoshimiLV2PluginUI::~YoshimiLV2PluginUI()
{
    if (uiHost.plugin_human_id != NULL)
    {
        free(const_cast<char *>(uiHost.plugin_human_id));
        uiHost.plugin_human_id = NULL;
    }
    _plugin->_synth->closeGui();
    Fl::check();
}


bool YoshimiLV2PluginUI::init()
{
    if (_plugin == NULL || uiHost.ui_closed == NULL)
        return false;

    _plugin->_synth->setGuiClosedCallback(YoshimiLV2PluginUI::static_guiClosed, this);

    return true;
}


LV2UI_Handle YoshimiLV2PluginUI::instantiate(const _LV2UI_Descriptor *descriptor, const char *plugin_uri, const char *bundle_path, LV2UI_Write_Function write_function, LV2UI_Controller controller, LV2UI_Widget *widget, const LV2_Feature * const *features)
{
    const _LV2UI_Descriptor *desc = descriptor;
    descriptor = desc;
    const char *plug = plugin_uri;
    plugin_uri = plug;
    // lines above suppress warnings - may use later

    YoshimiLV2PluginUI *uiinst = new YoshimiLV2PluginUI(bundle_path, write_function, controller, widget, features);
    if (uiinst->init())
    {
        return static_cast<LV2_External_UI_Widget *>(uiinst);
    }
    else
        delete uiinst;
    return NULL;

}


void YoshimiLV2PluginUI::cleanup(LV2UI_Handle ui)
{
    YoshimiLV2PluginUI *uiinst = static_cast<YoshimiLV2PluginUI *>(ui);
    delete uiinst;
}


void YoshimiLV2PluginUI::static_guiClosed(void *arg)
{
    static_cast<YoshimiLV2PluginUI *>(arg)->_masterUI = NULL;
    static_cast<YoshimiLV2PluginUI *>(arg)->_plugin->_synth->closeGui();
}


void YoshimiLV2PluginUI::run()
{
    if (_masterUI != NULL)
    {
        for (int i = 0; !_plugin->_synth->getRuntime().LogList.empty() && i < 5; ++i)
        {
            _masterUI->Log(_plugin->_synth->getRuntime().LogList.front());
            _plugin->_synth->getRuntime().LogList.pop_front();
        }
        Fl::check();

        GuiThreadMsg::processGuiMessages();

    }
    else
    {
        if (uiHost.ui_closed != NULL)
            uiHost.ui_closed(_controller);
    }
}


void YoshimiLV2PluginUI::show()
{
    _plugin->_synth->getRuntime().showGui = true;
    bool bInit = false;
    if (_masterUI == NULL)
        bInit = true;
    _masterUI = _plugin->_synth->getGuiMaster();
    if (_masterUI == NULL)
    {
        _plugin->_synth->getRuntime().Log("Failed to instantiate gui");
        return;
    }
    if (bInit)
    {
        Fl::lock();
        Fl::unlock();
        _masterUI->Init(uiHost.plugin_human_id);
    }

}


void YoshimiLV2PluginUI::hide()
{
    if (_masterUI)
        _masterUI->masterwindow->hide();
}


void YoshimiLV2PluginUI::static_Run(_LV2_External_UI_Widget *_this_)
{
    reinterpret_cast<_externalUI *>(_this_)->uiInst->run();

}


void YoshimiLV2PluginUI::static_Show(_LV2_External_UI_Widget *_this_)
{
    reinterpret_cast<_externalUI *>(_this_)->uiInst->show();
}


void YoshimiLV2PluginUI::static_Hide(_LV2_External_UI_Widget *_this_)
{
    reinterpret_cast<_externalUI *>(_this_)->uiInst->hide();

}



extern "C" const LV2_Descriptor *lv2_descriptor(uint32_t index)
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


int mainCreateNewInstance(unsigned int, bool) //stub
{
    return 0;
}


void mainRegisterAudioPort(SynthEngine *, int ) //stub
{

}
