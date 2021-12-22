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

#include <thread>
#include <mutex>
//#include <string>


namespace { // Implementation helpers...

    class TaskRunnerImpl
    {
        std::mutex mtx;
        using Guard = const std::lock_guard<std::mutex>;

        public:
            /* Meyer's Singleton */
            static TaskRunnerImpl& access()
            {
                static TaskRunnerImpl instance{};
                return instance;
            }

            void schedule(task::RunnerBackend::Task&& task)
            {
                Guard lock(mtx);
                UNIMPLEMENTED("actually schedule a task into a background thread");
            };

            void reschedule(task::RunnerBackend::Task&& task)
            {
                Guard lock(mtx);
                UNIMPLEMENTED("re-schedule a task, possibly with some penalty or wait time");
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
}

