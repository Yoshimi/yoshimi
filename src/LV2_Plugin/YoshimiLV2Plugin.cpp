/*
    YoshimiLV2Plugin

    Copyright 2014, Andrew Deryabin

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "YoshimiLV2Plugin.h"
#include "Misc/Config.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"
#include "MasterUI.h"
#include "Synth/BodyDisposal.h"
#include <math.h>


void YoshimiLV2Plugin::process(uint32_t sample_count)
{
    uint real_sample_count = min(sample_count, _bufferSize);
    uint32_t offs = 0;
    uint32_t next_frame = 0;
    float *tmpLeft [NUM_MIDI_PARTS + 1];
    float *tmpRight [NUM_MIDI_PARTS + 1];
    struct midi_event intMidiEvent;
    for(uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
    {
        tmpLeft [i] = lv2Left [i];
        if(tmpLeft [i] == NULL)
            tmpLeft [i] = zynLeft [i];
        tmpRight [i] = lv2Right [i];
        if(tmpRight [i] == NULL)
            tmpRight [i] = zynRight [i];
    }
    LV2_ATOM_SEQUENCE_FOREACH(_midiDataPort, event)
    {
        if(event == NULL)
            continue;
        if(event->body.size > sizeof(intMidiEvent.data))
            continue;

        if(event->body.type == _midi_event_id)
        {
            next_frame = event->time.frames;            
            if(next_frame >= real_sample_count)
                continue;
            uint32_t to_process = next_frame - offs;
            if(to_process > 0)
            {
                //_synth->buffersize_f = _synth->buffersize = to_process;
                //_synth->bufferbytes = to_process * sizeof(float);
                _synth->MasterAudio(tmpLeft, tmpRight, to_process);
                offs = next_frame;
                for(uint32_t i = 0; i < NUM_MIDI_PARTS + 1; ++i)
                {
                    tmpLeft [i] += offs;
                    tmpRight [i] += offs;
                }
            }
            //process this midi event
            const uint8_t *msg = (const uint8_t*)(event + 1);
            bool bMidiProcessed = false;
            if(_bFreeWheel != NULL)
            {
                if(*_bFreeWheel != 0)
                {
                    processMidiMessage(msg);
                    bMidiProcessed = true;
                }
            }
            if(!bMidiProcessed)
            {
                intMidiEvent.time = next_frame;
                memset(intMidiEvent.data, 0, sizeof(intMidiEvent.data));
                memcpy(intMidiEvent.data, msg, event->body.size);
                unsigned int wrote = 0;
                int tries = 0;
                while (wrote < sizeof(intMidiEvent) && tries < 3)
                {
                    int act_write = jack_ringbuffer_write(_midiRingBuf, reinterpret_cast<const char *>(&intMidiEvent), sizeof(intMidiEvent) - wrote);
                    wrote += act_write;
                    msg += act_write;
                    ++tries;
                }
                if (wrote == sizeof(struct midi_event))
                {
                    if (sem_post(&_midiSem) < 0)
                        _synth->getRuntime().Log("processMidi semaphore post error, "
                                    + string(strerror(errno)));
                }
                else
                {
                    _synth->getRuntime().Log("Bad write to midi ringbuffer: "
                                + asString(wrote) + " / "
                                + asString((int)sizeof(struct midi_event)));
                }

            }

        }
    }

    if(offs < real_sample_count)
    {
        uint32_t to_process = real_sample_count - offs;
        if(to_process > 0)
        {
            //_synth->buffersize_f = _synth->buffersize = to_process;
            //_synth->bufferbytes = to_process * sizeof(float);
            _synth->MasterAudio(tmpLeft, tmpRight, to_process);
            offs = next_frame;
        }

    }

}

void YoshimiLV2Plugin::processMidiMessage(const uint8_t * msg)
{
    unsigned char channel, note, velocity;
    int ctrltype;
    int par = 0;
    unsigned int ev;
    channel = msg[0] & 0x0F;
    switch ((ev = msg[0] & 0xF0))
    {
        case 0x01: // modulation wheel or lever
            ctrltype = C_modwheel;
            par = msg[2];
            setMidiController(channel, ctrltype, par);
            break;

        case 0x07: // channel volume (formerly main volume)
            ctrltype = C_volume;
            par = msg[2];
            setMidiController(channel, ctrltype, par);
            break;

        case 0x0B: // expression controller
            ctrltype = C_expression;
            par = msg[2];
            setMidiController(channel, ctrltype, par);
            break;

        case 0x78: // all sound off
            ctrltype = C_allsoundsoff;
            setMidiController(channel, ctrltype, 0);
            break;

        case 0x79: // reset all controllers
            ctrltype = C_resetallcontrollers;
            setMidiController(channel, ctrltype, 0);
            break;

        case 0x7B:  // all notes off
            ctrltype = C_allnotesoff;
            setMidiController(channel, ctrltype, 0);
            break;

        case 0x80: // note-off
            note = msg[1];
            setMidiNote(channel, note);
            break;

        case 0x90: // note-on
            if ((note = msg[1])) // skip note == 0
            {
                velocity = msg[2];
                setMidiNote(channel, note, velocity);
            }
            break;

        case 0xB0: // controller
            ctrltype = getMidiController(msg[1]);
            par = msg[2];
            setMidiController(channel, ctrltype, par);
            break;

        case 0xC0: // program change
            ctrltype = C_programchange;
            par = msg[1];
            setMidiProgram(channel, par);
            break;

        case 0xE0: // pitch bend
            ctrltype = C_pitchwheel;
            par = ((msg[2] << 7) | msg[1]) - 8192;
            setMidiController(channel, ctrltype, par);
            break;

        case 0xF0: // system exclusive
            break;

        default: // wot, more?
            synth->getRuntime().Log("other event: " + asString((int)ev));
            break;
    }

}

void *YoshimiLV2Plugin::midiThread()
{
    struct midi_event midiEvent;
    while (synth->getRuntime().runSynth)
    {
        if (sem_wait(&_midiSem) < 0)
        {
            _synth->getRuntime().Log("midiThread semaphore wait error, "
                        + string(strerror(errno)));
            continue;
        }
        if (!_synth->getRuntime().runSynth)
            break;
        size_t fetch = jack_ringbuffer_read(_midiRingBuf, (char*)&midiEvent, sizeof(struct midi_event));
        if (fetch != sizeof(struct midi_event))
        {
            _synth->getRuntime().Log("Short ringbuffer read, " + asString((float)fetch) + " / "
                        + asString((int)sizeof(struct midi_event)));
            continue;
        }
        processMidiMessage(reinterpret_cast<const uint8_t *>(midiEvent.data));
    }
    return NULL;
}

void *YoshimiLV2Plugin::idleThread()
{
    //temporary
    _synth->getRuntime().showGui = true;
    MasterUI *guiMaster = _synth->getGuiMaster();
    if (guiMaster == NULL)
    {
        _synth->getRuntime().Log("Failed to instantiate gui");
        return NULL;
    }
    guiMaster->Init("yoshimi lv2 plugin");

    while (_synth->getRuntime().runSynth)
    {
        _synth->getRuntime().deadObjects->disposeBodies();
        // where all the action is ...
        if(_synth->getRuntime().showGui)
            Fl::wait(0.033333);
        else
            usleep(33333);
    }
    return NULL;
}

YoshimiLV2Plugin::YoshimiLV2Plugin(SynthEngine *synth, double sampleRate, const char *bundlePath, const LV2_Feature *const *features):
    MusicIO(synth),
    _synth(synth),
    _sampleRate(static_cast<uint32_t>(sampleRate)),
    _bufferSize(0),
    _bundlePath(bundlePath),
    _midiDataPort(NULL),
    _midi_event_id(0),
    _bufferPos(0),
    _offsetPos(0),
    _bFreeWheel(NULL),
    _midiRingBuf(NULL),
    _pMidiThread(0),
    _pIdleThread(0)
{
    _uridMap.handle = NULL;
    _uridMap.map = NULL;
    const LV2_Feature *f = NULL;
    const LV2_Options_Option *options = NULL;
    while((f = *features) != NULL)
    {
        if(strcmp(f->URI, LV2_URID__map) == 0)
        {
            _uridMap = *(static_cast<LV2_URID_Map *>(f->data));
        }
        else if(strcmp(f->URI, LV2_OPTIONS__options) == 0)
        {
            options = static_cast<LV2_Options_Option *>(f->data);
        }
        ++features;
    }

    if(_uridMap.map != NULL && options != NULL)
    {
        _midi_event_id = _uridMap.map(_uridMap.handle, LV2_MIDI__MidiEvent);
        LV2_URID maxBufSz = _uridMap.map(_uridMap.handle, LV2_BUF_SIZE__maxBlockLength);
        LV2_URID minBufSz = _uridMap.map(_uridMap.handle, LV2_BUF_SIZE__minBlockLength);
        LV2_URID atomInt = _uridMap.map(_uridMap.handle, LV2_ATOM__Int);
        while(options->size > 0 && options->value != NULL)
        {
            if(options->context == LV2_OPTIONS_INSTANCE)
            {
                if((options->key == minBufSz || options->key == maxBufSz) && options->type == atomInt)
                {
                    uint32_t bufSz = *static_cast<const uint32_t *>(options->value);
                    if(_bufferSize < bufSz)
                        _bufferSize = bufSz;
                }
            }
            ++options;
        }

    }

    if(_bufferSize == 0)
        _bufferSize = 1024;
}

YoshimiLV2Plugin::~YoshimiLV2Plugin()
{
    if(_synth != NULL)
    {
        _synth->getRuntime().runSynth = false;        
        sem_post(&_midiSem);
        sem_wait(&_midiSem);
        sem_destroy(&_midiSem);
        if(_midiRingBuf != NULL)
        {
            jack_ringbuffer_free(_midiRingBuf);
            _midiRingBuf = NULL;
        }
        pthread_join(_pMidiThread, NULL);
        pthread_join(_pIdleThread, NULL);
        delete _synth;
        _synth = NULL;
    }
}

bool YoshimiLV2Plugin::init()
{
    if(_uridMap.map == NULL || _sampleRate == 0 || _bufferSize == 0 || _midi_event_id == 0)
        return false;
    if(!prepBuffers(false))
        return false;
    if(sem_init(&_midiSem, 0, 0) != 0)
    {
        _synth->getRuntime().Log("Failed to create midi semaphore");
        return false;
    }

    _midiRingBuf = jack_ringbuffer_create(sizeof(struct midi_event) * 4096);
    if (!_midiRingBuf)
    {
        _synth->getRuntime().Log("Failed to create midi ringbuffer");
        return false;
    }
    if (jack_ringbuffer_mlock(_midiRingBuf))
    {
        _synth->getRuntime().Log("Failed to lock memory");
        return false;
    }

    _synth->Init(_sampleRate, _bufferSize);

    memset(lv2Left, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));
    memset(lv2Right, 0, sizeof(float *) * (NUM_MIDI_PARTS + 1));

    _synth->getRuntime().runSynth = true;

    if(!_synth->getRuntime().startThread(&_pMidiThread, YoshimiLV2Plugin::static_midiThread, this, true, 1))
    {
        synth->getRuntime().Log("Failed to start midi thread");
        return false;
    }

    if(!_synth->getRuntime().startThread(&_pIdleThread, YoshimiLV2Plugin::static_idleThread, this, false, 0))
    {
        synth->getRuntime().Log("Failed to start idle thread");
        return false;
    }


    return true;
}




LV2_Handle	YoshimiLV2Plugin::instantiate (const struct _LV2_Descriptor *, double sample_rate, const char *bundle_path, const LV2_Feature *const *features)
{
    SynthEngine *synth = new SynthEngine(0, NULL, true);
    if(synth == NULL)
        return NULL;
    YoshimiLV2Plugin *inst = new YoshimiLV2Plugin(synth, sample_rate, bundle_path, features);
    if(inst->init())
        return static_cast<LV2_Handle>(inst);
    else
        delete inst;
    return NULL;
}

void YoshimiLV2Plugin::connect_port(LV2_Handle instance, uint32_t port, void *data_location)
{
    if(port > NUM_MIDI_PARTS + 2)
        return;
     YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
     if(port == 0)//atom midi event port
     {
         inst->_midiDataPort = static_cast<LV2_Atom_Sequence *>(data_location);
         return;
     }else if(port == 1) //freewheel control port
     {
         inst->_bFreeWheel = static_cast<float *>(data_location);
         return;
     }

     port -=2;

     if(port == 0) //main outl
         port = NUM_MIDI_PARTS * 2;
     else if(port == 1) //main outr
         port = NUM_MIDI_PARTS * 2 + 1;
     else
         port -= 2;


     int portIndex = static_cast<int>(floorf((float)port/2.0f));
     if(port % 2 == 0) //left channel
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


/*
LV2_Worker_Interface yoshimi_wrk_iface =
{
    YoshimiLV2Plugin::lv2wrk_work,
    YoshimiLV2Plugin::lv2wrk_response,
    YoshimiLV2Plugin::lv2_wrk_end_run
};
*/

const void *YoshimiLV2Plugin::extension_data(const char *uri)
{
    /*if(strcmp(uri, LV2_WORKER__interface) == 0)
        return static_cast<const void *>(&yoshimi_wrk_iface);*/
    return NULL;
}

void *YoshimiLV2Plugin::static_midiThread(void *arg)
{
    return static_cast<YoshimiLV2Plugin *>(arg)->midiThread();
}

void *YoshimiLV2Plugin::static_idleThread(void *arg)
{
    return static_cast<YoshimiLV2Plugin *>(arg)->idleThread();
}
/*
LV2_Worker_Status YoshimiLV2Plugin::lv2wrk_work(LV2_Handle instance, LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle, uint32_t size, const void *data)
{

}

LV2_Worker_Status YoshimiLV2Plugin::lv2wrk_response(LV2_Handle instance, uint32_t size, const void *body)
{

}

LV2_Worker_Status YoshimiLV2Plugin::lv2_wrk_end_run(LV2_Handle instance)
{

}

*/


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

extern "C" const LV2_Descriptor *lv2_descriptor(uint32_t index)
{
    switch(index)
    {
    case 0:
        return &yoshimi_lv2_desc;
    default:
        break;
    }
    return NULL;
}

bool mainCreateNewInstance() //stub
{
    return true;
}
