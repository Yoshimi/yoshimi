/*
    BuildScheduler.h - running wavetable building tasks in background

    Copyright 2021,  Ichthyostega

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef BUILDSCHEDULER_H
#define BUILDSCHEDULER_H

//#include "globals.h"

#include <atomic>
#include <future>
#include <utility>
#include <optional>
#include <functional>
#include <stdexcept>

//using std::string;
using std::move;


//////////////////////TODO work in progress
#include <string>

class ToDo : public std::logic_error
{
public:
    ToDo(std::string msg) :
        std::logic_error{"UNIMPLEMENTED: "+msg}
    { }
};

#define UNIMPLEMENTED(_MSG_) \
    throw ToDo(_MSG_)
//////////////////////TODO work in progress



/* »Result« type — with the ability to yield "no result"
 *
 * NOTE: starting with C++17 we should use just std::optional here.
 */
template<class VAL>
class Optional
{
    struct EmptyPlaceholder { };
    union {
        EmptyPlaceholder empty;
        VAL resultVal;
    };
    bool hasResult;


    Optional()
        : empty()
        , hasResult{false}
    { }
public:
    static const Optional EMPTY;

    Optional(VAL&& result)
        : resultVal{move(result)}
        , hasResult{true}
    { }
    // Note: Optional<VAL> is copyable or movable, whenever VAL is.


    explicit operator bool()  const
    {
        return hasResult;
    }

    VAL& operator*()
    {
        if (not hasResult)
            throw std::logic_error{"Accessing empty result value"};
        return resultVal;
    }
    VAL const& operator*()  const
    {
        if (not hasResult)
            throw std::logic_error{"Accessing empty result value"};
        return resultVal;
    }
};

/* marker for the »missing result« */
template<class VAL>
const Optional<VAL> Optional<VAL>::EMPTY{};





/* An optional link to a data value under construction.
 * The data value (template Parameter TAB) is assumed to be produced
 * by a function running in some background thread or task scheduler.
 * FutureBuild is the front-end to be used by Synth code to deal with
 * such a value, and manage re-building of that value on demand.
 *
 * Usage Rules
 * - On construction, actual Scheduler implementation must be supplied (as Lambda)
 * - Whenever a new Data element must be built, invoke requestNewBuild()  (idempotent function)
 * - Test if a build is underway with the bool conversion (or isUnderway())
 * - Test if the new value is ready and can be retrieve without blocking: isReady()
 * - Blocking wait for the value to become ready: call blockingWait();
 * - Retrieve the value and reset all state atomically: swap(existingTab)
 *
 */
template<class TAB>
class FutureBuild
{
    // Type abbreviations
    using FutureVal = std::future<TAB>;
    using Promise   = std::promise<TAB>;

    using BuildOp = std::function<TAB()>;

    /* the managed data value under construction */
    std::atomic<FutureVal*> target{nullptr};

    /* request new build and abort existing one */
    std::atomic<bool> dirty{false};


    //--Customisation---
    using ScheduleAction   = std::function<FutureVal()>;

    ScheduleAction   schedule;

    public:
       ~FutureBuild();
        FutureBuild(ScheduleAction schedFun)
           : schedule{move(schedFun)}
        { }

        // shall not be copied or moved or assigned
        FutureBuild(FutureBuild&&)                 = delete;
        FutureBuild(FutureBuild const&)            = delete;
        FutureBuild& operator=(FutureBuild&&)      = delete;
        FutureBuild& operator=(FutureBuild const&) = delete;


        // state information functions
        bool isRequested()  const;
        bool isUnderway()  const;
        bool isReady()  const;

        explicit operator bool()  const { return isUnderway(); }


        // mutating operations
        void requestNewBuild();
        void swap(TAB & dataToReplace);

    private:
};




namespace task {
    /* Access point to a global generic task runner backend */
    class RunnerBackend
    {
        public:
            using Task = std::function<void()>;

            static void schedule(Task&&);
            static void reschedule(Task&&);
    };

    /* Global facility to manage building actions as background task.
     * When constructing a concrete FutureBuild instance, this front-end shall be used
     * to wire the actual BuildOperation and turn it into a simple function to be scheduled.
     */
    template<class TAB>
    class BuildScheduler
    {
        // Type abbreviations
        using FutureVal = std::future<TAB>;
        using Promise   = std::promise<TAB>;

        using OptionalResult = Optional<TAB>;
        using BuildOperation = std::function<OptionalResult()>;
        using ScheduleAction = std::function<FutureVal()>;

        private:
            static RunnerBackend::Task buildTaskForScheduler(BuildOperation&& buildOp, Promise&& promise)
            {
                return [buildOp=move(buildOp),
                        promise=move(promise)]() -> void
                        {// This code will run within the scheduler/task
                            try {
                                OptionalResult result = buildOp();

                                if (result)
                                {   // Computation successful; push result into connected future
                                    promise.set_value(move(*result));
                                    return;
                                }
                            }
                            catch(...)
                            {
                                std::exception_ptr failure = std::current_exception();
                                promise.set_exception(failure);
                                return;
                            }

                            // computation was marked as /aborted/
                            // Thus use the exiting functor and promise
                            // to package them into a new task for rescheduling...
                            RunnerBackend::Task followUpTask = buildTaskForScheduler(move(buildOp),
                                                                                     move(promise));
                            RunnerBackend::reschedule(move(followUpTask));
                        };
            }

        public:
            static ScheduleAction wireBuildFunction(BuildOperation buildOp)
            {
                return [buildOp=move(buildOp)]()
                        {// This code will run whenever the FutureBuild wants to schedule another BuildOperation...
                            Promise promise;
                            FutureVal future = promise.get_future();

                            // Package the BuildOperation into a generic functor
                            RunnerBackend::Task task = buildTaskForScheduler(move(buildOp), move(promise));

                            // now pass this packaged Task to the Task-Runner backend...
                            RunnerBackend::schedule(move(task));

                            // hand-over the corresponding future to FutureBuild
                            return future;
                        };
            }
    };


}//(End)namespace task


/* === Implementation of FutureBuild API functions === */

template<class TAB>
bool FutureBuild<TAB>::isRequested()  const
{
    UNIMPLEMENTED("thread save evaluation: was a new build / abort requested?");
}

template<class TAB>
bool FutureBuild<TAB>::isUnderway()  const
{
    UNIMPLEMENTED("thread save evaluation: do we currently have an active build task running, or a result ready to receive?");
}

template<class TAB>
bool FutureBuild<TAB>::isReady()  const
{
    UNIMPLEMENTED("thread save evaluation: is there a new build result ready to be picked up, without blocking?");
}



template<class TAB>
void FutureBuild<TAB>::requestNewBuild()
{
    UNIMPLEMENTED("Thread-save idempotent operation: set a flag to cause a new build to be launched; possibly terminate an existing build beforehand");
}

template<class TAB>
void FutureBuild<TAB>::swap(TAB & dataToReplace)
{
    UNIMPLEMENTED("Thread-save mutator: pick up the result and exchange it with the old value. Discard old value then. Blocks if result is not yet ready");
}

template<class TAB>
FutureBuild<TAB>::~FutureBuild()
{
    if (target.load(std::memory_order_seq_cst))
        delete target;
}



#endif /*BUILDSCHEDULER_H*/
