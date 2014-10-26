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

MusicClient *musicClient = NULL;

MusicClient *MusicClient::newMusicClient(SynthEngine *_synth)
{
    MusicClient *musicObj = NULL;

    return musicObj;
}


YoshimiLV2Plugin::YoshimiLV2Plugin(double sampleRate, const char *bundlePath):
   _sampleRate(sampleRate),
   _bundlePath(bundlePath)

{
    //_synth = new SynthEngine(0, NULL, true);
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

    //if(!Runtime.Setup(0, NULL))
//        return false;

    return true;
}


LV2_Handle	yoshimiInstantiate (const struct _LV2_Descriptor *descriptor, double sample_rate, const char *bundle_path, const LV2_Feature *const *features)
{
   YoshimiLV2Plugin *inst = new YoshimiLV2Plugin(sample_rate, bundle_path);
   return static_cast<LV2_Handle>(inst);
}

LV2_Descriptor yoshimi_lv2_desc =
{
   "http://yoshimi.sourceforge.net/lv2_plugin",
   yoshimiInstantiate,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
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
