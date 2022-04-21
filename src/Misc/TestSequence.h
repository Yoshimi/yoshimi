/*
    TestSequence.h - helper for automated testing of note sequences

    Copyright 2022, Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License (version 2
    or later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef TEST_SEQUENCE_H
#define TEST_SEQUENCE_H

#include <string>
#include <cassert>
#include <functional>
#include <vector>
#include <memory>
#include <cmath>

#include "globals.h"


namespace test {

/**
 * Arrangement of Test events to be carried out for a single test cycle.
 * This is a timeline of events, and each "tick" on the timeline corresponds
 * to a "calculate buffer" call into the SynthEngine. "Events" are arbitrary
 * functors, which are to be invoked _before_ calculating the associated number
 * of buffers of sound. This arrangement allows to _plan_ notes as pair of
 * "noteOn"/"noteOff" event, and then to retrieve the resulting operation
 * sequence broken down to distinct tick counts and ready for execution.
 */
class TestSequence
{
    public:
        using Event = std::function<void()>;

        struct EventStep
        {
            Event event;
            size_t step;

            EventStep(Event e, size_t s)
                : event{move(e)}
                , step{s}
            { }
        };
        using EventSeq = std::vector<EventStep>;

        TestSequence(size_t cntTicks)
            : maxTicks{cntTicks}
            , events{}
        { }

        void addNote(Event,Event, float hold, float offset=0);
        void addEvent(Event, float offset);

        using Iterator = EventSeq::const_iterator;
        Iterator begin() const { return events.begin();}
        Iterator end()   const { return events.end();  }

        size_t size()    const { return events.size(); }

    private:
        size_t maxTicks;
        EventSeq events;

        size_t clamped(size_t tickNo) const { return std::min(maxTicks, std::max(tickNo, size_t{0})); }
        size_t quantise(float fract)  const { return clamped(ceilf(fract * maxTicks));                }
};


/**
 * Base operation: Plant an arbitrary "Event" into the test timeline.
 */
inline void TestSequence::addEvent(Event event, float offset)
{
    size_t preTicks = quantise(offset);
    Iterator it = begin();
    while (it != end())
    {
        EventStep& precursor = const_cast<EventStep&>(*it);
        if (precursor.step <= preTicks)
        {// Event to be located beyond current EventStep
            preTicks -= precursor.step;
            ++it;
            continue;
        }
        // Event must be located within current EvetnStep's range
        assert(preTicks < precursor.step);
        size_t postTicks = precursor.step - preTicks;
        precursor.step = preTicks;              // shorten precursor
        events.emplace(++it, event, postTicks); // insert before next
        return;
    }
    if (preTicks > 0)
        events.emplace_back([]{/*do nothing*/}, preTicks);
    events.emplace_back(event, maxTicks - quantise(offset));
}


/**
 * Plant a note into the test timeline.
 * Start and duration are given as fraction of the (fixed) overall timeline length,
 * and all internal accounting is done in "ticks" (each tick corresponds to a SynthEngine compute call).
 * The note will be started with the onEvent and ended by the offEvent, thereby possibly filling or
 * separating any intervals already present in the TestSequence.
 */
inline void TestSequence::addNote(Event onEvent, Event offEvent, float hold, float offset)
{
    addEvent(onEvent,  offset);
    addEvent(offEvent, offset+hold);
}


}// namespace test
#endif /*TEST_SEQUENCE_H*/
