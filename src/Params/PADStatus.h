/*
    PADStatus.h - Status of PADSynth wavetable building

    Copyright 2022,  Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License (version 2
    or later) for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef PAD_STATUS_H
#define PAD_STATUS_H

#include "globals.h"
#include "Interface/InterChange.h"
#include "Misc/FormatFuncs.h"

#include <functional>
#include <string>
#include <array>


/**
 * Display current wavetable build status and send status updates to the UI.
 * While the BuildScheduler coordinates the re-generation of Wavetables by Inverse Fast Fourier Transform,
 * the PADnoteUI shall display a live status indicator, so the user knows when actually to expect a sonic
 * change. This functionality is comprised of two parts, which can be accessed through this unified front-end.
 * - code within the build process generates status update messages, which are sent into the Queue "toGUI()",
 *   which can be accessed via the InterChange instance. Note: this happens concurrently from various threads.
 * - the PADnoteUI receives and integrates all those status message into a single synthetic global status,
 *   and changes the relevant embedded widgets accordingly
 * The PADStatus *instance* is the object located within the UI to receive and integrate status messages.
 */
class PADStatus
{
public:
    enum Stage {
        CLEAN = 0,
        FADING,
        PENDING,
        BUILDING,
        DIRTY,
    };

private:
    using HandlerFun = std::function<void()>;
    using HandlerTab = std::array<HandlerFun, DIRTY+1>;

    HandlerTab handler;
    const uchar partID;
    const uchar kitID;


public:
    PADStatus(uchar part, uchar kit)
        : handler{}
        , partID{part}
        , kitID{kit}
    { }
   ~PADStatus() = default;

    // shall not be copied or moved or assigned
    PADStatus(PADStatus&&)                 = delete;
    PADStatus(PADStatus const&)            = delete;
    PADStatus& operator=(PADStatus&&)      = delete;
    PADStatus& operator=(PADStatus const&) = delete;

    // install actual handler function(s)
    void on(Stage stage, HandlerFun fun)
    {
        handler[stage] = fun;
    }

    // activate the transition to given new status
    void activateStage(Stage newStage)
    {
        if (handler[newStage])
            handler[newStage]();
    }

    void handleStateMessage(CommandBlock const& stateMsg)
    {
        if (stateMsg.data.control == PADSYNTH::control::applyChanges
            and stateMsg.data.part == partID
            and stateMsg.data.kit  == kitID
           )
            activateStage(Stage(stateMsg.data.offset));
    }


    // generate Status message within SynthEngine...
    static void mark(Stage newStage, InterChange&, uchar,uchar);
};



/**
 * Cast a state message towards UI threadsafe and asynchronously.
 * @param interChange the access point to command message handling
 */
inline void PADStatus::mark(Stage newStage, InterChange& interChange, uchar partID, uchar kitID)
{
    CommandBlock stateMsg;

    stateMsg.data.type    = TOPLEVEL::type::Integer;
    stateMsg.data.control = PADSYNTH::control::applyChanges;
    stateMsg.data.engine  = PART::engine::padSynth;
    stateMsg.data.source  = TOPLEVEL::action::lowPrio | TOPLEVEL::action::noAction;
    stateMsg.data.offset  = newStage;
    //
    stateMsg.data.part      = partID;
    stateMsg.data.kit       = kitID;
    //
    stateMsg.data.insert    = UNUSED;
    stateMsg.data.parameter = UNUSED;
    stateMsg.data.miscmsg   = UNUSED;
    stateMsg.data.spare0    = UNUSED;
    stateMsg.data.spare1    = UNUSED;
    stateMsg.data.value     = 0;

#ifdef GUI_FLTK
    bool success = interChange.toGUI.write(stateMsg.bytes);
    if (not success)
        interChange.Log("Failure PADStatus sending toGUI: newStage="+func::asString(newStage));
#endif
}

#endif /*PAD_STATUS_H*/
