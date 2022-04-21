/*
    EffectMgr.cpp - Effect manager, an interface between the program and effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2017-2019, Will Godfrey

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code.

*/

#include <iostream>

#include "Misc/SynthEngine.h"
#include "Effects/EffectMgr.h"

EffectMgr::EffectMgr(const bool insertion_, SynthEngine *_synth) :
    Presets{_synth},
    efxoutl{size_t(_synth->buffersize)},
    efxoutr{size_t(_synth->buffersize)},
    insertion{insertion_},
    filterpars{NULL},
    nefx{TOPLEVEL::insert::none},
    dryonly{false},
    efx{}
{
    setpresettype("Peffect");
    defaults();
}




void EffectMgr::defaults(void)
{
    changeeffect(TOPLEVEL::insert::none);
    setdryonly(false);
}


// Change the effect
void EffectMgr::changeeffect(int _nefx)
{
    cleanup();
    if (nefx == _nefx)
        return;
    nefx = _nefx;
    switch (nefx)
    {
        case TOPLEVEL::insert::reverb:
            efx.reset(new Reverb{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::echo:
            efx.reset(new Echo{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::chorus:
            efx.reset(new Chorus{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::phaser:
            efx.reset(new Phaser{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::alienWah:
            efx.reset(new Alienwah{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::distortion:
            efx.reset(new Distorsion{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::eq:
            efx.reset(new EQ{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

        case TOPLEVEL::insert::dynFilter:
            efx.reset(new DynamicFilter{insertion, efxoutl.get(), efxoutr.get(), synth});
            break;

            // put more effect here
        default:
            efx.reset();
            break; // no effect (thru)
    }
    if (efx)
        filterpars = efx->filterpars;
}


// Obtain the effect number
int EffectMgr::geteffect(void)
{
    return (nefx);
}


// Cleanup the current effect
void EffectMgr::cleanup(void)
{
    memset(efxoutl.get(), 0, synth->bufferbytes);
    memset(efxoutr.get(), 0, synth->bufferbytes);
    if (efx)
        efx->cleanup();
}


// Get the preset of the current effect
unsigned char EffectMgr::getpreset(void)
{
    if (efx)
    {
        //cout << "Effect preset " << int(efx->Ppreset) << endl;
        return efx->Ppreset;
    }
    else
    {
        //cout << "No effect" << endl;
        return 0;
    }
}


// Change the preset of the current effect
void EffectMgr::changepreset(unsigned char npreset)
{
    if (efx)
        efx->setpreset(npreset);
}


// Change a parameter of the current effect
void EffectMgr::seteffectpar(int npar, unsigned char value)
{
    if (!efx)
        return;
    efx->changepar(npar, value);
}


// Get a parameter of the current effect
unsigned char EffectMgr::geteffectpar(int npar)
{
    if (!efx)
        return 0;
    return efx->getpar(npar);
}


// Apply the effect
void EffectMgr::out(float *smpsl, float *smpsr)
{
    if (!efx)
    {
        if (!insertion)
        {
            memset(smpsl, 0, synth->sent_bufferbytes);
            memset(smpsr, 0, synth->sent_bufferbytes);
            memset(efxoutl.get(), 0, synth->sent_bufferbytes);
            memset(efxoutr.get(), 0, synth->sent_bufferbytes);
        }
        return;
    }
    memset(efxoutl.get(), 0, synth->sent_bufferbytes);
    memset(efxoutr.get(), 0, synth->sent_bufferbytes);
    efx->out(smpsl, smpsr);

    if (nefx == TOPLEVEL::insert::eq)
    {   // this is need only for the EQ effect
        memcpy(smpsl, efxoutl.get(), synth->sent_bufferbytes);
        memcpy(smpsr, efxoutr.get(), synth->sent_bufferbytes);
        return;
    }

    // Insertion effect
    if (insertion != 0)
    {
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float volume = efx->volume.getAndAdvanceValue();
            float v1, v2;
            if (volume < 0.5f)
            {
                v1 = 1.0f;
                v2 = volume * 2.0f;
            } else {
                v1 = (1.0f - volume) * 2.0f;
                v2 = 1.0f;
            }
            if (nefx == 1 || nefx==2)
                v2 *= v2; // for Reverb and Echo, the wet function is not linear

            if (dryonly)
            {
                // this is used for instrument effect only
                smpsl[i] *= v1;
                smpsr[i] *= v1;
                efxoutl[i] *= v2;
                efxoutr[i] *= v2;
            } else {
                // normal instrument/insertion effect
                smpsl[i] = smpsl[i] * v1 + efxoutl[i] * v2;
                smpsr[i] = smpsr[i] * v1 + efxoutr[i] * v2;
            }
        }
    } else { // System effect
        for (int i = 0; i < synth->sent_buffersize; ++i)
        {
            float volume = efx->volume.getAndAdvanceValue();
            efxoutl[i] *= 2.0f * volume;
            efxoutr[i] *= 2.0f * volume;
            smpsl[i] = efxoutl[i];
            smpsr[i] = efxoutr[i];
        }
    }
}


// Get the effect volume for the system effect
float EffectMgr::sysefxgetvolume(void)
{
    return (!efx) ? 1.0f : efx->outvolume.getValue();
}


// Get the EQ response
float EffectMgr::getEQfreqresponse(float freq)
{
    return  (nefx == 7) ? efx->getfreqresponse(freq) : 0.0f;
}


void EffectMgr::setdryonly(bool value)
{
    dryonly = value;
}


void EffectMgr::add2XML(XMLwrapper *xml)
{
    xml->addpar("type", geteffect());

    if (!efx || !geteffect())
        return;
    xml->addpar("preset", efx->Ppreset);

    xml->beginbranch("EFFECT_PARAMETERS");
    for (int n = 0; n < 128; ++n)
    {   // \todo evaluate who should oversee saving and loading of parameters
        int par = geteffectpar(n);
        if (par == 0)
            continue;
        xml->beginbranch("par_no", n);
        xml->addpar("par", par);
        xml->endbranch();
    }
    if (filterpars)
    {
        xml->beginbranch("FILTER");
        filterpars->add2XML(xml);
        xml->endbranch();
    }
    xml->endbranch();
}


void EffectMgr::getfromXML(XMLwrapper *xml)
{
    changeeffect(xml->getpar127("type", geteffect()));
    if (!efx || !geteffect())
        return;
    changepreset(xml->getpar127("preset", efx->Ppreset));

    bool isChanged = false;
    if (xml->enterbranch("EFFECT_PARAMETERS"))
    {
        for (int n = 0; n < 128; ++n)
        {
            int par = geteffectpar(n); // find default
            seteffectpar(n, 0); // erase effect parameter
            if (xml->enterbranch("par_no", n) == 0)
                continue;
            seteffectpar(n, xml->getpar127("par", par));
            if (par != geteffectpar(n))
            {
                isChanged = true;
                //cout << "changed par " << n << endl;
                //may use this later to ID
            }
            xml->exitbranch();
        }
        seteffectpar(-1, isChanged);
        if (filterpars)
        {
            if (xml->enterbranch("FILTER"))
            {
                filterpars->getfromXML(xml);
                xml->exitbranch();
            }
        }
        xml->exitbranch();
        //if (geteffectpar(-1))
            //cout << "Some pars changed" << endl;
    }
    cleanup();
}


float LimitMgr::geteffectlimits(CommandBlock *getData)
{
    int effType = getData->data.kit & 127;
    float value = 0;
    switch (effType)
    {
        case TOPLEVEL::insert::none:
            value = 0;
            break;
        case TOPLEVEL::insert::reverb:
            Revlimit reverb;
            value = reverb.getlimits(getData);
            break;
        case TOPLEVEL::insert::echo:
            Echolimit echo;
            value = echo.getlimits(getData);
            break;
        case TOPLEVEL::insert::chorus:
            Choruslimit chorus;
            value = chorus.getlimits(getData);
            break;
        case TOPLEVEL::insert::phaser:
            Phaserlimit phaser;
            value = phaser.getlimits(getData);
            break;
        case TOPLEVEL::insert::alienWah:
            Alienlimit alien;
            value = alien.getlimits(getData);
            break;
        case TOPLEVEL::insert::distortion:
            Distlimit dist;
            value = dist.getlimits(getData);
            break;
        case TOPLEVEL::insert::eq:
            EQlimit EQ;
            value = EQ.getlimits(getData);
            break;
        case TOPLEVEL::insert::dynFilter:
            Dynamlimit dyn;
            value = dyn.getlimits(getData);
            break;
        default:
            value = TOPLEVEL::insert::count - TOPLEVEL::insert::none;
            break;
    }
    return value;
}
