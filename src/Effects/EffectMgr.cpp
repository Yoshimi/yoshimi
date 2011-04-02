/*
    EffectMgr.cpp - Effect manager, an interface betwen the program and effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2009 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code, modified April 2011
*/

#include <fftw3.h>

#include "Misc/SynthEngine.h"
#include "Effects/EffectMgr.h"

EffectMgr::EffectMgr(const bool insertion_) :
    insertion(insertion_),
    filterpars(NULL),
    nefx(0),
    efx(NULL),
    dryonly(false)
{
    setpresettype("Peffect");
    efxoutl = (float*)fftwf_malloc(synth->bufferbytes);
    efxoutr = (float*)fftwf_malloc(synth->bufferbytes);
    memset(efxoutl, 0, synth->bufferbytes);
    memset(efxoutr, 0, synth->bufferbytes);
    defaults();
}


EffectMgr::~EffectMgr()
{
    if (efx)
        delete efx;
    fftwf_free(efxoutl);
    fftwf_free(efxoutr);
}


void EffectMgr::defaults(void)
{
    changeeffect(0);
    setdryonly(false);
}


// Change the effect
void EffectMgr::changeeffect(int _nefx)
{
    cleanup();
    if (nefx == _nefx)
        return;
    nefx = _nefx;
    memset(efxoutl, 0, synth->bufferbytes);
    memset(efxoutr, 0, synth->bufferbytes);
    if (efx)
        delete efx;
    switch (nefx)
    {
        case 1:
            efx = new Reverb(insertion, efxoutl, efxoutr);
            break;
        case 2:
            efx = new Echo(insertion, efxoutl, efxoutr);
            break;
        case 3:
            efx = new Chorus(insertion, efxoutl, efxoutr);
            break;
        case 4:
            efx = new Phaser(insertion, efxoutl, efxoutr);
            break;
        case 5:
            efx = new Alienwah(insertion, efxoutl, efxoutr);
            break;
        case 6:
            efx = new Distorsion(insertion, efxoutl, efxoutr);
            break;
        case 7:
            efx = new EQ(insertion, efxoutl, efxoutr);
            break;
        case 8:
            efx = new DynamicFilter(insertion, efxoutl, efxoutr);
            break;
            // put more effect here
        default:
            efx = NULL;
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
    if (efx)
        efx->cleanup();
}


// Get the preset of the current effect
unsigned char EffectMgr::getpreset(void)
{
    if (efx)
        return efx->Ppreset;
    else
        return 0;
}


// Change the preset of the current effect
void EffectMgr::changepreset_nolock(unsigned char npreset)
{
    if (efx)
        efx->setpreset(npreset);
}


// Change the preset of the current effect(with thread locking)
void EffectMgr::changepreset(unsigned char npreset)
{
    synth->actionLock(lock);
    changepreset_nolock(npreset);
    synth->actionLock(unlock);
}


// Change a parameter of the current effect
void EffectMgr::seteffectpar_nolock(int npar, unsigned char value)
{
    if (!efx)
        return;
    efx->changepar(npar, value);
}


// Change a parameter of the current effect (with thread locking)
void EffectMgr::seteffectpar(int npar, unsigned char value)
{
    synth->actionLock(lock);
    seteffectpar_nolock(npar, value);
    synth->actionLock(unlock);
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
            memset(smpsl, 0, synth->bufferbytes);
            memset(smpsr, 0, synth->bufferbytes);
            memset(efxoutl, 0, synth->bufferbytes);
            memset(efxoutr, 0, synth->bufferbytes);
        }
        return;
    }
    memset(efxoutl, 0, synth->bufferbytes);
    memset(efxoutr, 0, synth->bufferbytes);
    efx->out(smpsl, smpsr);

    float volume = efx->volume;

    if (nefx == 7)
    {   // this is need only for the EQ effect
        // aca: another memcpy() candidate
        //for (int i = 0; i < synth->buffersize; ++i)
        //{
         //   smpsl[i] = efxoutl[i];
         //   smpsr[i] = efxoutr[i];
        //}
        memcpy(smpsl, efxoutl, synth->bufferbytes);
        memcpy(smpsr, efxoutr, synth->bufferbytes);
        return;
    }

    // Insertion effect
    if (insertion != 0)
    {
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
            v2 *= v2; // for Reverb and Echo, the wet function is not liniar

        if (dryonly)
        {   // this is used for instrument effect only
            for (int i = 0; i < synth->buffersize; ++i)
            {
                smpsl[i] *= v1;
                smpsr[i] *= v1;
                efxoutl[i] *= v2;
                efxoutr[i] *= v2;
            }
        } else {
            // normal instrument/insertion effect
            for (int i = 0; i < synth->buffersize; ++i)
            {
                smpsl[i] = smpsl[i] * v1 + efxoutl[i] * v2;
                smpsr[i] = smpsr[i] * v1 + efxoutr[i] * v2;
            }
        }
    } else { // System effect
        for (int i = 0; i < synth->buffersize; ++i)
        {
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
    return (!efx) ? 1.0f : efx->outvolume;
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
    efx->Ppreset = xml->getpar127("preset", efx->Ppreset);

    if (xml->enterbranch("EFFECT_PARAMETERS"))
    {
        for (int n = 0; n < 128; ++n)
        {
            seteffectpar_nolock(n, 0); // erase effect parameter
            if (xml->enterbranch("par_no", n) == 0)
                continue;
            int par = geteffectpar(n);
            seteffectpar_nolock(n, xml->getpar127("par", par));
            xml->exitbranch();
        }
        if (filterpars)
        {
            if (xml->enterbranch("FILTER"))
            {
                filterpars->getfromXML(xml);
                xml->exitbranch();
            }
        }
        xml->exitbranch();
    }
    cleanup();
}
