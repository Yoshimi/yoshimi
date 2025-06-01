/*
    VerInfo.h - Program version data

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2025,      Ichthyostega

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


#ifndef VER_INFO_H
#define VER_INFO_H

#include "globals.h"

#include <string>

/**
 * Program Version information
 * Used for compatibility checks of persisted data (XML)
 */
struct VerInfo
{
    const uint maj{0};
    const uint min{0};
    const uint rev{0};

    VerInfo() = default;

    explicit VerInfo(uint major
                    ,uint minor = 0
                    ,uint revision = 0)
        : maj{major}, min{minor}, rev{revision}
        { }

    /** parse dot separated version spec -> Config.cpp */
    VerInfo(std::string const& spec);

    // standard copy operations acceptable


    explicit operator bool()  const
    {
        return maj > 0 or min > 0;
    }

    friend bool operator==(VerInfo const& v1, VerInfo const& v2)
    {
        return v1.maj == v2.maj
           and v1.min == v2.min
           and v1.rev == v2.rev;
    }

    friend bool operator!=(VerInfo const& v1, VerInfo const& v2)
    {
        return not (v1 == v2);
    }

    friend bool operator< (VerInfo const& v1, VerInfo const& v2)
    {
        return  v1.maj  < v2.maj
            or (v1.maj == v2.maj and v1.min < v2.min)
            or (v1.min == v2.min and v1.rev < v2.rev);
    }

    friend bool is_equivalent (VerInfo const& v1, VerInfo const& v2)
    {
        return v1.maj == v2.maj
           and v1.min == v2.min;
    }


    /** forcibly replace this VersionInfo with the given other version */
    void forceReset(VerInfo const& changedVersion)
    {
        new(this) VerInfo{changedVersion};
    }// re-construct in-place, since const data fields can not be assigned
};

#endif /*VER_INFO_H*/
