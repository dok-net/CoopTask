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

#include "CoopTaskBase.h"
#include "circular_queue/circular_queue.h"

/// A semaphore that is safe to use from CoopTasks.
/// Only post() is safe to use from interrupt service routines,
/// or concurrent OS threads that must synchronized with the singled thread running CoopTasks.
class CoopSemaphore
{
protected:
    std::atomic<unsigned> value;
    std::atomic<CoopTaskBase*> pendingTask0;
    circular_queue<CoopTaskBase*> pendingTasks;

    // capture-less functions for iterators.
    static void awakeAndSchedule(CoopTaskBase*&& task)
    {
        task->scheduleTask(true);
    }
    static bool notIsSelfTask(CoopTaskBase*& task)
    {
        return CoopTaskBase::self() != task;
    }

    /// @param withDeadline true: the ms parameter specifies the relative timeout for a successful
    /// aquisition of the semaphore.
    /// false: there is no deadline, the ms parameter is disregarded.
    /// @param ms the relative timeout measured in milliseconds.
    /// @returns: true if it sucessfully acquired the semaphore, either immediately or after sleeping.
    /// false if the deadline expired, or the maximum number of pending tasks is exceeded.
    bool _wait(const bool withDeadline = false, const uint32_t ms = 0);

public:
    /// @param val the initial value of the semaphore.
    /// @param maxPending the maximum supported number of concurrently waiting tasks.
    CoopSemaphore(unsigned val, size_t maxPending = 10) : value(val), pendingTask0(nullptr), pendingTasks(maxPending) {}
    CoopSemaphore(const CoopSemaphore&) = delete;
    CoopSemaphore& operator=(const CoopSemaphore&) = delete;
    ~CoopSemaphore()
    {
        // wake up all queued tasks
        pendingTasks.for_each(awakeAndSchedule);
    }

    /// post() is the only operation that is allowed from an interrupt service routine,
    /// or a concurrent OS thread that is synchronized with the singled thread running CoopTasks.
    bool IRAM_ATTR post();

    /// @param newVal: the semaphore is immediately set to the specified value. if newVal is greater
    /// than the current semaphore value, the behavior is identical to as many post operations.
    bool setval(unsigned newVal);

    /// @returns: true if it sucessfully acquired the semaphore, either immediately or after sleeping.
    /// false if the maximum number of pending tasks is exceeded.
    bool wait()
    {
        return _wait();
    }

    /// @param ms the relative timeout, measured in milliseconds, for a successful aquisition of the semaphore.
    /// @returns: true if it sucessfully acquired the semaphore, either immediately or after sleeping.
    /// false if the deadline expired, or the maximum number of pending tasks is exceeded.
    bool wait(uint32_t ms)
    {
        return _wait(true, ms);
    }

    /// @returns: true if the semaphore was acquired immediately, otherwise false.
    bool try_wait();
};

#endif // __CoopSemaphore_h
