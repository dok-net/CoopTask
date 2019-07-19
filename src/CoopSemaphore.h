/*
CoopSemaphore.h - Implementation of a semaphore for cooperative scheduling tasks
Copyright (c) 2019 Dirk O. Kaar. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __CoopSemaphore_h
#define __CoopSemaphore_h

#include "CoopTask.h"
#include "circular_queue/circular_queue.h"

class CoopSemaphore
{
protected:
    unsigned value;
    std::unique_ptr<circular_queue<CoopTask*>> pendingTasks;
public:
    /// @param val the initial value of the semaphore
    /// @param maxPending the maximum supported number of concurrently waiting tasks
    CoopSemaphore(unsigned val, unsigned maxPending = 10) : value(val), pendingTasks(new circular_queue<CoopTask*>(maxPending)) {}
    CoopSemaphore(const CoopSemaphore&) = delete;
    CoopSemaphore& operator=(const CoopSemaphore&) = delete;
    ~CoopSemaphore()
    {
        // wake up all queued tasks
        pendingTasks->for_each([](CoopTask* task) { task->sleep(false); });
    }
    bool post()
    {
        if (value++) return true;
        if (pendingTasks->available()) {
            --value;
            pendingTasks->pop()->sleep(false);
        }
        return true;
    }
    // @returns: true if sucessfully aquired the semaphore, either immediately or after sleeping. false if maximum number of pending tasks is exceeded.
    bool wait()
    {
        if (value)
        {
            --value;
            return true;
        }
        if (!pendingTasks->push(&CoopTask::self())) return false;
        CoopTask::sleep();
        return true;
    }
    bool try_wait()
    {
        if (value)
        {
            --value;
            return true;
        }
        return false;
    }
};

#endif // __CoopSemaphore_h
