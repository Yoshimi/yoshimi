/*
    BuildScheduler.cpp -  implementation details of background wavetable building

    Copyright 2021,  Ichthyostega.

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

#include "Misc/BuildScheduler.h"

#include <chrono>
#include <thread>
#include <mutex>
#include <queue>


namespace { // Implementation details of scheduling...

    /* »dirty wait delay« : when further rebuilds are requested while
     * a background build process is underway, an additional grace period
     * is added to allow for more changes to trickle in and avoid overloading
     * the system with lots of rescheduling tasks. */
    constexpr auto RESCHEDULE_DELAY = std::chrono::milliseconds(50);

    /* number of threads to keep free as headroom for the Synth */
    const size_t REQUIRED_HEADROOM = 2;

    /* factor to overload the nominally available CPUs */
    const double OVERPROVISIONING = 1.5;

    size_t determineUsableBackgroundConcurrency()
    {
        size_t cpuCount = std::thread::hardware_concurrency();
        int free = cpuCount * OVERPROVISIONING - REQUIRED_HEADROOM;
        return std::max(free, 1);
    }



    class TaskRunnerImpl
    {
        std::mutex mtx;
        using Guard = const std::lock_guard<std::mutex>;

        using Task = task::RunnerBackend::Task;

        std::queue<Task> waitingTasks{};

        static const size_t THREAD_LIMIT;
        size_t runningThreads = 0;

        public:
            /* Meyer's Singleton */
            static TaskRunnerImpl& access()
            {
                static TaskRunnerImpl instance{};
                return instance;
            }

            /* Implementation of scheduling into background thread:
             * pass the work task through a queue and start up to
             * THREAD_LIMIT workers to consume those work tasks. */
            void schedule(Task&& task)
            {
                Guard lock(mtx);
                waitingTasks.push(move(task));
                if (runningThreads < THREAD_LIMIT)
                    launchWorker();
            };

            void reschedule(Task&& task)
            {
                Task delayedTask{
                    [workOp = move(task)] () -> void
                        {// this code runs within a worker thread
                            std::this_thread::sleep_for(RESCHEDULE_DELAY);
                            workOp();
                        }};
                schedule(move(delayedTask));
            }

        private:
            void markWorker_finished()
            {
                Guard lock(mtx);
                if (runningThreads == 0)
                    throw std::logic_error("BuildScheduler: worker thread management floundered");
                --runningThreads;
            }

            void launchWorker()
            {
                // note: mutex locked at caller
                std::thread backgroundThread(
                    [this] () -> void
                        {// worker thread(s): consume queue contents
                            while (Task workOp = pullFromQueue())
                                try {
                                    workOp();
                                }
                                catch(...)
                                {/* absorb failure in workOp */}
                            markWorker_finished();
                        });
                backgroundThread.detach();
                assert(runningThreads < THREAD_LIMIT);
                ++runningThreads;
            }

            Task pullFromQueue()
            {
                Guard lock(mtx);
                if (waitingTasks.empty())
                    return Task(); // empty Task to signal end
                Task nextWorkOp(move(waitingTasks.front()));
                waitingTasks.pop();
                return nextWorkOp;
            }
    };

    const size_t TaskRunnerImpl::THREAD_LIMIT = determineUsableBackgroundConcurrency();

}//(End)Implementation details of scheduling.



namespace task {

    /* === Implementation of access to the task runner === */

    void RunnerBackend::schedule(Task&& task)
    {
        TaskRunnerImpl::access().schedule(move(task));
    }

    void RunnerBackend::reschedule(Task&& task)
    {
        TaskRunnerImpl::access().reschedule(move(task));
    }

    void dirty_wait_delay()
    {
        std::this_thread::sleep_for(RESCHEDULE_DELAY);
    }
}
