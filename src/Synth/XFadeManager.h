/*
    XFadeManager.h - support for cross-fading wavetables

    Copyright 2022, Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef X_FADE_MANAGER_H
#define X_FADE_MANAGER_H

#include <memory>
#include <utility>
#include <cassert>


/**
 * Manage an ongoing crossfade.
 * During XFade, the WaveInterpolators within each active PADnote will be
 * replaced by a cross-fading variant, which also refers to the previously existing
 * wavetable(s) -- so this component serves to...
 * - indicate that there is an ongoing XFade
 * - prevent/delay the next XFade until the current is done
 * - manage the storage of the old wavetable during XFade
 * @note for this to work, actual cross-fading calculations must detect the fact of
 *       an ongoing crossfade and then #attachFader() and #detachFader() when done.
 * @warning the ref-count in this class is deliberately *not thread-safe* (to avoid
 *       thread synchronisation overhead). If we ever start processing the SynthEngine
 *       concurrently, this whole logic will break and needs to be revised!
 * @tparam WAV actual data type of the wavetable to be managed
 * @see PADnote::computeNoteParameters()
 */
template<class WAV>
class XFadeManager
{
    std::unique_ptr<WAV> oldTable{};
    int useCnt{0};

    public:
        XFadeManager()  = default;
       ~XFadeManager()  = default;

        // shall not be copied or moved or assigned
        XFadeManager(XFadeManager&&)                 = delete;
        XFadeManager(XFadeManager const&)            = delete;
        XFadeManager& operator=(XFadeManager&&)      = delete;
        XFadeManager& operator=(XFadeManager const&) = delete;


        /** is there an active ongoing crossfade? */
        explicit operator bool()  const
        { return bool(oldTable); }


        /** Take ownership of the old Wavetable,
         *  unless there is already an ongoing crossfade.
         * @return `true` if given wavetable was moved and a crossfade shall start
         */
        bool startXFade(WAV& existingOldTable)
        {
            if (oldTable and useCnt > 0)
                return false;
            oldTable.reset(new WAV{std::move(existingOldTable)});
            useCnt = 0;
            return true;
        }

        void attachFader()
        {
            ++useCnt;
        }

        void detachFader()
        {
            --useCnt;
            checkUsage();
        }

        void checkUsage()
        {
            if (oldTable and useCnt <= 0)
            {
                oldTable.reset();
                useCnt = 0;
            }
        }
};


#endif /*X_FADE_MANAGER_H*/
