/*
    EffectMgr.cpp - Effect manager, an interface betwen the program and effects

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009, Alan Calvert

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

    This file is a derivative of the ZynAddSubFX original, modified October 2009
*/

#include <iostream>

using namespace std;

#include "Misc/Master.h"
#include "Effects/EffectMgr.h"

EffectMgr::EffectMgr(const bool insertion_) :
    insertion(insertion_),
    filterpars(NULL),
    nefx(0),
    efx(NULL),
    dryonly(false)
{
    setPresetType("Peffect");
    efxoutl = new float [buffersize];
    efxoutr = new float [buffersize];
    memset(efxoutl, 0, buffersize * sizeof(float));
    memset(efxoutr, 0, buffersize * sizeof(float));
    setDefaults();
}


EffectMgr::~EffectMgr()
{
    if (efx != NULL)
        delete efx;
    delete [] efxoutl;
    delete [] efxoutr;
}

void EffectMgr::setDefaults(void)
{
    changeEffect(0);
    setDryOnly(false);
}

// Change the effect
void EffectMgr::changeEffect(int _nefx)
{
    Cleanup();
    if (nefx == _nefx)
        return;
    nefx = _nefx;
    memset(efxoutl, 0, buffersize * sizeof(float));
    memset(efxoutr, 0, buffersize * sizeof(float));
    if (efx != NULL)
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
    if (efx != NULL)
        filterpars = efx->filterpars;
}

// Obtain the effect number
int EffectMgr::getEffect(void)
{
    return (nefx);
}

// Cleanup the current effect
void EffectMgr::Cleanup(void)
{
    if (efx != NULL)
        efx->Cleanup();
}


// Get the preset of the current effect
unsigned char EffectMgr::getPreset(void)
{
    if (efx != NULL)
        return efx->Ppreset;
    else
        return 0;
}

// Change the preset of the current effect
void EffectMgr::changePreset_nolock(unsigned char npreset)
{
    if (efx != NULL)
        efx->setPreset(npreset);
}

// Change the preset of the current effect(with thread locking)
void EffectMgr::changePreset(unsigned char npreset)
{
    zynMaster->actionLock(lock);
    changePreset_nolock(npreset);
    zynMaster->actionLock(unlock);
}


// Change a parameter of the current effect
void EffectMgr::setEffectPar_nolock(int npar, unsigned char value)
{
    if (efx == NULL)
        return;
    efx->changePar(npar, value);
}

// Change a parameter of the current effect (with thread locking)
void EffectMgr::setEffectPar(int npar, unsigned char value)
{
    zynMaster->actionLock(lock);
    setEffectPar_nolock(npar, value);
    zynMaster->actionLock(unlock);
}

// Get a parameter of the current effect
unsigned char EffectMgr::getEffectPar(int npar)
{
    if (efx == NULL)
        return 0;
    return efx->getPar(npar);
}


// Apply the effect
void EffectMgr::out(float *smpsl, float *smpsr)
{
    if (efx == NULL)
    {
        if (insertion == 0)
        {
            memset(smpsl, 0, buffersize * sizeof(float));
            memset(smpsr, 0, buffersize * sizeof(float));
            memset(efxoutl, 0, buffersize * sizeof(float));
            memset(efxoutr, 0, buffersize * sizeof(float));
        }
        return;
    }
    memset(efxoutl, 0, buffersize * sizeof(float));
    memset(efxoutr, 0, buffersize * sizeof(float));
    efx->out(smpsl, smpsr);

    float volume = efx->volume;

    if (nefx == 7)
    {   // this is need only for the EQ effect
        // aca: another memcpy() candidate
        for (int i = 0; i < buffersize; ++i)
        {
            smpsl[i] = efxoutl[i];
            smpsr[i] = efxoutr[i];
        }
        return;
    }

    // Insertion effect
    if (insertion != 0)
    {
        float v1, v2;
        if (volume < 0.5)
        {
            v1 = 1.0;
            v2 = volume * 2.0;
        } else {
            v1 = (1.0 - volume) * 2.0;
            v2 = 1.0;
        }
        if (nefx == 1 || nefx==2)
            v2 *= v2; // for Reverb and Echo, the wet function is not liniar

        if (dryonly)
        {   // this is used for instrument effect only
            for (int i = 0; i < buffersize; ++i)
            {
                smpsl[i] *= v1;
                smpsr[i] *= v1;
                efxoutl[i] *= v2;
                efxoutr[i] *= v2;
            }
        } else {
            // normal instrument/insertion effect
            for (int i = 0; i < buffersize; ++i)
            {
                smpsl[i] = smpsl[i] * v1 + efxoutl[i] * v2;
                smpsr[i] = smpsr[i] * v1 + efxoutr[i] * v2;
            }
        }
    } else { // System effect
        for (int i = 0; i < buffersize; ++i)
        {
            efxoutl[i] *= 2.0 * volume;
            efxoutr[i] *= 2.0 * volume;
            smpsl[i] = efxoutl[i];
            smpsr[i] = efxoutr[i];
        }
    }
}

// Get the effect volume for the system effect
float EffectMgr::sysefxGetVolume(void)
{
    return (efx == NULL) ? 1.0 : efx->outvolume;
}


// Get the EQ response
float EffectMgr::getEQfreqresponse(float freq)
{
    return  (nefx == 7) ? efx->getFreqResponse(freq) : 0.0;
}


void EffectMgr::setDryOnly(bool value)
{
    dryonly = value;
}

void EffectMgr::add2XML(XMLwrapper *xml)
{
    xml->addpar("type", getEffect());

    if (efx == NULL || getEffect() == 0)
        return;
    xml->addpar("preset", efx->Ppreset);

    xml->beginbranch("EFFECT_PARAMETERS");
    for (int n = 0; n < 128; ++n)
    {   // \todo evaluate who should oversee saving and loading of parameters
        int par = getEffectPar(n);
        if (par == 0)
            continue;
        xml->beginbranch("par_no", n);
        xml->addpar("par", par);
        xml->endbranch();
    }
    if (filterpars != NULL)
    {
        xml->beginbranch("FILTER");
        filterpars->add2XML(xml);
        xml->endbranch();
    }
    xml->endbranch();
}

void EffectMgr::getfromXML(XMLwrapper *xml)
{
    changeEffect(xml->getpar127("type",getEffect()));
    if (efx == NULL || getEffect() == 0)
        return;
    efx->Ppreset = xml->getpar127("preset", efx->Ppreset);

    if (xml->enterbranch("EFFECT_PARAMETERS"))
    {
        for (int n = 0; n < 128; ++n)
        {
            setEffectPar_nolock(n, 0); // erase effect parameter
            if (xml->enterbranch("par_no", n) == 0)
                continue;
            int par = getEffectPar(n);
            setEffectPar_nolock(n, xml->getpar127("par", par));
            xml->exitbranch();
        }
        if (filterpars != NULL)
        {
            if (xml->enterbranch("FILTER"))
            {
                filterpars->getfromXML(xml);
                xml->exitbranch();
            }
        }
        xml->exitbranch();
    }
    Cleanup();
}
