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

}

YoshimiLV2Plugin::YoshimiLV2Plugin(SynthEngine *synth, double sampleRate, const char *bundlePath, const LV2_Feature *const *features):
    MusicIO(synth),
    _synth(synth),
    _sampleRate(static_cast<uint32_t>(sampleRate)),
    _bufferSize(0),
    _bundlePath(bundlePath),
    midiDataPort(NULL),
    _midi_event_id(0),
    _bufferPos(0)
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
        delete _synth;
        _synth = NULL;
    }
}

bool YoshimiLV2Plugin::init()
{
    if(_uridMap.map == NULL || _sampleRate == 0 || _bufferSize == 0 || _midi_event_id == 0)
        return false;
    _synth->Init(_sampleRate, _bufferSize);
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
    if(port > NUM_MIDI_PARTS + 1)
        return;
     YoshimiLV2Plugin *inst = static_cast<YoshimiLV2Plugin *>(instance);
     if(port == 0)//atom midi event port
     {
         inst->midiDataPort = data_location;
         return;
     }
     --port;

     int portIndex = static_cast<int>(floorf((float)port/2.0f));
     if(port % 2 == 0) //left channel
         inst->zynLeft[portIndex] = static_cast<float *>(data_location);
     else
         inst->zynRight[portIndex] = static_cast<float *>(data_location);

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

const void *YoshimiLV2Plugin::extension_data(const char *)
{
    return NULL;
}



LV2_Descriptor yoshimi_lv2_desc =
{
    "http://yoshimi.sourceforge.net/lv2_plugin",
    YoshimiLV2Plugin::instantiate,
    YoshimiLV2Plugin::connect_port,
    YoshimiLV2Plugin::activate,
    YoshimiLV2Plugin::run,
    YoshimiLV2Plugin::deactivate,
    YoshimiLV2Plugin::cleanup,
    NULL
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
