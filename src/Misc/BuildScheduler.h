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

#include <atomic>
#include <future>
#include <utility>
#include <optional>
#include <functional>
#include <stdexcept>
#include <cassert>

using std::move;
using std::optional;






/* Workaround for a long-standing problem in C++ : std::function binding move-only values.
 * This problem notoriously appears when dealing with std::promise in "Task"-Functions.
 * The official solution is proposed for C++23 (std::move_only_function).
 *
 * Explanation: std::function requires its target to be /copyable/; whenever we bind some
 * target functor (e.g. a Lambda function) into a std::function capsule, the code for a
 * copy constructor will be generated (even if this code is never used), leading to
 * compilation failure, whenever the lambda captures a move-only type.
 *
 * This adapter encapsulates and forwards to the embedded type, but provides a "fake"
 * copy constructor. The intention is to never actually use that copy-ctor, and as a
 * safety feature, the implementation will terminate the program when invoked.
 */
template<typename M>
class FakeCopyAdapter
{
    using Payload = std::decay_t<M>;
    Payload payload;

    static Payload&& must_not_be_called()
    {   assert(not "Copy constructor must not be invoked");
        std::terminate();
    }

public:
    template<typename X>
    FakeCopyAdapter(X&& initialiser)
        : payload(std::forward<X>(initialiser))
    { }

    operator Payload& ()             { return payload; }
    Payload& operator* ()            { return payload; }
    Payload* operator->()            { return &payload;}
    operator Payload const& () const { return payload; }
    Payload const& operator*() const { return payload; }
    Payload const* operator->()const { return &payload;}

    FakeCopyAdapter()                                  = default;
    FakeCopyAdapter(FakeCopyAdapter &&)                = default;
    FakeCopyAdapter& operator=(FakeCopyAdapter&&)      = default;
    FakeCopyAdapter& operator=(FakeCopyAdapter const&) = delete;

    FakeCopyAdapter(FakeCopyAdapter const&) noexcept
        : FakeCopyAdapter{must_not_be_called()}
    { }
};




/* Thread-safe optional link to a data value under construction.
 * The data value (template Parameter TAB) is expected to be produced
 * by a function running in some background thread or task scheduler.
 * FutureBuild is the front-end to be used by Synth code to deal with
 * such a value, and manage re-building of that value on demand.
 *
 * Usage Rules
 * - On construction, actual Scheduler implementation must be supplied (as Lambda)
 * - Whenever a new Data element must be built, invoke requestNewBuild()  (idempotent function)
 * - Test if a build is underway with the bool conversion (or isUnderway())
 * - Test if the new value is ready and can be retrieved without blocking: isReady()
 * - Blocking wait for the value to become ready: call blockingWait();
 * - Retrieve the value and reset all state atomically: swap(existingTab)
 *
 * Remark: while this class was designed for use by PADSynth, in fact
 * there is no direct dependency; it is sufficient that there is some
 * background operation function, which returns a (movable) TAB value.
 * Likewise, there is no direct dependency to the actual scheduler.
 */
template<class TAB>
class FutureBuild
{
    // Type abbreviations
    using FutureVal = std::future<TAB>;
    using ResultVal = std::optional<TAB>;
    using BuildOp = std::function<ResultVal()>;

    /* the managed data value under construction */
    std::atomic<FutureVal*> target{nullptr};

    /* request new build and abort existing one */
    std::atomic<bool> dirty{false};


    //--Customisation---
    using ScheduleAction = std::function<FutureVal()>;
    using SchedulerSetup = std::function<ScheduleAction(BuildOp)>;

    ScheduleAction schedule;

    public:
       ~FutureBuild();
        FutureBuild(SchedulerSetup setupScheduler, BuildOp backgroundAction)
           : schedule{setupScheduler(wireState(backgroundAction))}
        { }

        // shall not be copied or moved or assigned
        FutureBuild(FutureBuild&&)                 = delete;
        FutureBuild(FutureBuild const&)            = delete;
        FutureBuild& operator=(FutureBuild&&)      = delete;
        FutureBuild& operator=(FutureBuild const&) = delete;


        // state information functions
        bool shallRebuild()  const;
        bool isUnderway()  const;
        bool isReady()  const;

        explicit operator bool()  const { return isUnderway(); }

        // mutating operations
        void requestNewBuild();
        void swap(TAB & dataToReplace);

        void blockingWait(bool publishResult =false);

    private:
        BuildOp wireState(BuildOp);
        FutureVal* retrieveLatestTarget();
        bool installNewBuildTarget(FutureVal*);
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

    /* Add a fixed sleep period; related to the duration of a "dirty wait".
     * The latter is imposed when new parameter changes invalidate an ongoing
     * build, since typically further subsequent changes will arrive from GUI. */
    void dirty_wait_delay();


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

        using OptionalResult = optional<TAB>;
        using BuildOperation = std::function<OptionalResult()>;
        using ScheduleAction = std::function<FutureVal()>;

        private:
            struct PackagedBuildOperation
            {
                BuildOperation buildOp;
                FakeCopyAdapter<Promise> promise;

                void operator() ()
                {// This code will run within the scheduler/task
                    try {
                        OptionalResult result = buildOp();

                        if (result)
                        {   // Computation successful; push result into connected future
                            promise->set_value(move(*result));
                            return;
                        }
                    }
                    catch(...)
                    {
                        std::exception_ptr failure = std::current_exception();
                        promise->set_exception(failure);
                        return;
                    }

                    // computation was marked as /aborted/
                    // Thus use the exiting functor and promise
                    // to package them into a new task for rescheduling...
                    RunnerBackend::Task followUpTask = PackagedBuildOperation{move(buildOp),
                                                                              move(*promise)};
                    RunnerBackend::reschedule(move(followUpTask));
                }
            };

        public:
            static ScheduleAction wireBuildFunction(BuildOperation buildOp)
            {
                return [buildOp]()
                        {// This code will run whenever the FutureBuild wants to schedule another BuildOperation...
                            Promise promise;
                            FutureVal future = promise.get_future();

                            // pass BuildOperation to the Task-Runner backend, packaged as generic functor...
                            RunnerBackend::schedule(PackagedBuildOperation{move(buildOp), move(promise)});

                            // hand-over the corresponding future to FutureBuild
                            return future;
                        };
            }
    };

}//(End)namespace task




/* === Implementation of FutureBuild API functions === */

/* Thread safe evaluation: was a new build / abort requested? */
template<class TAB>
bool FutureBuild<TAB>::shallRebuild()  const
{
    return dirty.load(std::memory_order_relaxed);
}

/* thread safe evaluation: do we currently have an active build task scheduled? */
template<class TAB>
bool FutureBuild<TAB>::isUnderway()  const
{
    // changed curly braces below to normal ones to suppress warnings (was this intended?) - Will
    //return bool{target.load(std::memory_order_consume)}
    return bool(target.load(std::memory_order_consume))
        or shallRebuild();
}

/* Thread safe evaluation: is there a new build result ready to be picked up, without blocking?
 * Note: technically, if the value is not ready yet, the current thread might be blocked and
 * rescheduled immediately. There is no guaranteed wait-free status check for futures. */
template<class TAB>
bool FutureBuild<TAB>::isReady()  const
{
    FutureVal* future = target.load(std::memory_order_acquire);
    return future
       and future->wait_for(std::chrono::microseconds(0)) == std::future_status::ready;
}



/* Internal helper: Link the backgroundAction with the internal state management.
 * On construction, a function to schedule background actions is passed as extension point.
 * This scheduler call will be setup such as to invoke the (likewise customisable) background
 * action, and to control and manage this background scheduling is the purpose of this class.
 * The thread safe internal state management however requires that the backgroundAction itself
 * flips the "dirty" flag in a thread-safe way whenever it starts -- which can be linked in by
 * wrapping the action with this helper function, thereby keeping the flag an internal detail.
 */
template<class TAB>
typename FutureBuild<TAB>::BuildOp FutureBuild<TAB>::wireState(BuildOp backgroundAction)
{
    return [this, buildOp = move(backgroundAction)] () -> ResultVal
                  {// This code will run scheduled into a background thread...
                      bool expectTrue{true};
                      if (not dirty.compare_exchange_strong(expectTrue, false, std::memory_order_acq_rel))
                          throw std::logic_error("FutureBuild state handling logic broken: dirty flag was false. "
                                                 "Before a background task starts, the 'dirty' flag must be set "
                                                 "and will be cleared synchronised with the start of the task.");

                      // invoke background action...
                      return buildOp();
                  };
}


/* Thread-safe idempotent operation: cause a new build to be launched;
 * possibly terminate an existing build beforehand (by setting "dirty").
 * Note: setting dirty with compare-and-swap establishes a fence, which
 * only one thread can pass, and thus no one can set the target pointer,
 * after we have loaded and found it to be NULL. */
template<class TAB>
void FutureBuild<TAB>::requestNewBuild()
{
    bool expectFalse{false};
    if (not dirty.compare_exchange_strong(expectFalse, true, std::memory_order_acq_rel))
        return; // just walk away since dirty flag was set already...

    if (target.load(std::memory_order_acquire))
        return; // already running background task will see the dirty flag,
                // then abort and restart itself and clear the flag

    // If we reach this point, we are the first ones to set the dirty flag
    // and we can be sure there is currently no background task underway...
    // Launch a new background task, which on start clears the dirty flag.
    if (not installNewBuildTarget(new FutureVal{move(schedule())}))
        throw std::logic_error("FutureBuild state handling logic broken: "
                               "concurrent attempt to start a build, causing data corruption.");
}



/* internal helper: atomically install a new future to represent an ongoing build.
 * Returns false if no new future could be installed because there was an existing one.
 */
template<class TAB>
bool FutureBuild<TAB>::installNewBuildTarget(FutureVal* newBuild)
{
    FutureVal* expectedState = nullptr;
    return target.compare_exchange_strong(expectedState, newBuild
                                         ,std::memory_order_release);
}

/* internal helper: get the latest version of the future and atomically empty the pointer.
 * Implemented by looping until we're able to fetch a stable pointer value and swap the
 * pointer to NULL. Guarantees
 * - if the returned pointer is NULL, target /was already NULL/
 * - if the returned pointer is non-NULL, no one else can/could fetch it, and target is NULL now.
 */
template<class TAB>
typename FutureBuild<TAB>::FutureVal* FutureBuild<TAB>::retrieveLatestTarget()
{
    FutureVal* future = target.load(std::memory_order_acquire);
    while (future and not target.compare_exchange_strong(future, nullptr, std::memory_order_acq_rel))
    { } // loop until we got the latest pointee and could atomically set the pointer to NULL
    return future;
}


/* Thread-safe mutator: pick up the result and exchange it with the old value.
 * Reset state and discard old value then. Blocks if result is not yet ready.
 * WARNING: FutureBuild<TAB>::swap() must not be called concurrently, otherwise
 *          the whole state handling logic can break, causing multiple builds
 *          to be triggered at the same time and other horrible races. */
template<class TAB>
void FutureBuild<TAB>::swap(TAB & dataToReplace)
{
    FutureVal* future = retrieveLatestTarget();
    bool needReschedule = shallRebuild();
    if (future)
    {
        using std::swap;
        TAB newData{future->get()};   // may block until value is ready
        swap(dataToReplace, newData);
        delete future;
    }
    // we do not know if the "dirty" state was set before we picked up the future,
    // or afterwards. In the latter case, a new build could already be underway,
    // but it is impossible to detect that from here without a race. Fortunately,
    // this discrepancy can be "absorbed" by just calling requestNewBuild(),
    // since there a new build will be started only when necessary and atomically.
    if (needReschedule
            and not target.load(std::memory_order_relaxed))
    {
        // temporarily clear "dirty" flag to allow us to get into requestNewBuild();
        // the fence when setting "dirty", followed by target.load() ensures atomicity.
        dirty.store(false, std::memory_order_release);
        requestNewBuild();
    }
}

template<class TAB>
void FutureBuild<TAB>::blockingWait(bool publishResult)
{
    // possibly wait until the actual background task was started
    while (dirty.load(std::memory_order_relaxed) and not target.load(std::memory_order_relaxed))
        task::dirty_wait_delay();

    FutureVal* future = retrieveLatestTarget();
    if (future)
    {
        future->wait(); // blocks until result is ready

        // we alone hold the result now; attempt to publish it for the SynthEngine
        if (not publishResult or not installNewBuildTarget(future))
             delete future; // obsolete since other background build was triggered since our wait
    }
}


template<class TAB>
FutureBuild<TAB>::~FutureBuild()
{
    FutureVal* future = retrieveLatestTarget();
    if (future and future->valid())
    {// indicates active background task (result not yet reaped)
        future->wait(); // blocking wait until background task has finished
        delete future;
    }
}


#endif /*BUILDSCHEDULER_H*/
