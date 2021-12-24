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


namespace { // Implementation helpers...

    /* »dirty wait delay« : when further rebuilds are requested while
     * a background build process is underway, an additional grace period
     * is added to allow for more changes to trickle in and avoid overloading
     * the system with lots of rescheduling tasks. */
    constexpr auto RESCHEDULE_DELAY = std::chrono::milliseconds(50);


    class TaskRunnerImpl
    {
        std::mutex mtx;
        using Guard = const std::lock_guard<std::mutex>;

        using Task = task::RunnerBackend::Task;

        public:
            /* Meyer's Singleton */
            static TaskRunnerImpl& access()
            {
                static TaskRunnerImpl instance{};
                return instance;
            }

            /* Simplistic implementation of scheduling into background thread;
             * launch a new thread for each call, without imposing resource limits. */
            void schedule(Task&& task)
            {
                Guard lock(mtx);
                std::thread backgroundThread(move(task));
                backgroundThread.detach();
            };

            void reschedule(Task&& task)
            {
                Task delayedTask{[workOp = move(task)] () -> void
                                    {
                                        std::this_thread::sleep_for(RESCHEDULE_DELAY);
                                        workOp();
                                    }};
                schedule(move(delayedTask));
            }
    };
}//(End)Implementation helpers.



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
