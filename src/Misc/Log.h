/*
    Log.h - Interface for logging and error messages

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

*/

/*
 * NOTE: this interface is experimental as of 4/2025
 *
 * Up to now, the "interface" for logging was SynthEngine, rsp. the "runtime"
 * (Config object) accessible for each instance. Which is problematic, because
 * - Logging as such has no relation to the task of sound synthesis,
 *   rather it is a framework concern (cross-cutting concern).
 * - in many cases, the habit to log through SynthEngine is the sole reason
 *   why a pointer or reference to the latter is wired almost everywhere.
 * - Yoshimi can run several instances of the SynthEngine (either stand-alone
 *   or to accommodate several LV2 plug-in instances). In almost all cases,
 *   a specific link to some instance is not required for logging, which
 *   is used mostly for error messages, and at times for diagnostics.
 * - the way how log messages are passed from a SynthEngine- or background
 *   thread to the GUI-Thread (the "Main-Thread") is not threadsafe.
 *
 * The intention is first to establish a generic logger interface, distinct
 * from the SynthEngine. As a second step the backing infrastructure
 * could then be consolidated (Idea: use a ringbuffer)
 */

#ifndef MISC_LOG_H
#define MISC_LOG_H

#include "globals.h"

#include <string>
#include <utility>
#include <functional>

class Config;

/**
 * Interface to send an information or error message.
 * Logger is copyable / assignable.
 * The actual instance can be retrieved from Config / via a Synth-instance
 */
class Logger
{
    using Handler = std::function<void(std::string const&, char)>;

    Handler handler;

    Logger(Handler handlerSetup)
        : handler{std::move(handlerSetup)}
        { }
    // shall be created by Config / the "Synth runtime"
    friend class Config;

public:
    void operator() (std::string const& msg, char toStdErr =_SYS_::LogNormal)  const
    {
        handler(msg, toStdErr);
    }
};


#endif /*MISC_LOG_H*/
