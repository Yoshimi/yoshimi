/*
    Filter_.h - This class is inherited by filter classes

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

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

#ifndef FILTER__H
#define FILTER__H

class Filter_
{
    public:
        Filter_() { };
        virtual ~Filter_() { };
        virtual void filterout(float *smp) = 0;
        virtual void setfreq(float frequency) = 0;
        virtual void setfreq_and_q(float frequency, float q_) = 0;
        virtual void setq(float q_) = 0;
        virtual void setgain(float dBgain) { };
        float outgain;
};


#endif

