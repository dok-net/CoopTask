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
#ifdef ESP8266
#include <interrupts.h>
#endif
#include <atomic>

#if !defined(ESP32) && !defined(ESP8266)
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#endif

/// A semaphore that is safe to use from CoopTasks, or a thread that runs
/// mutually exclusive to all CoopTasks using the same CoopSemaphore.
/// Additionally, post() is safe to use from interrupt service routines,
/// the other, potentially blocking, operations naturally must not be used from one.
class CoopSemaphore
{
protected:
    std::atomic<unsigned> value;
    std::unique_ptr<circular_queue<CoopTask*>> pendingTasks;

public:
    /// @param val the initial value of the semaphore.
    /// @param maxPending the maximum supported number of concurrently waiting tasks.
    CoopSemaphore(unsigned val, unsigned maxPending = 10) : value(val), pendingTasks(new circular_queue<CoopTask*>(maxPending)) {}
    CoopSemaphore(const CoopSemaphore&) = delete;
    CoopSemaphore& operator=(const CoopSemaphore&) = delete;
    ~CoopSemaphore()
    {
        // wake up all queued tasks
        pendingTasks->for_each([](CoopTask* task) { task->sleep(false); });
    }
    /// post() is the only operation that is allowed from an interrupt service routine.
    bool IRAM_ATTR post()
    {
        unsigned val = 0;
#ifndef ESP8266
        while (!value.compare_exchange_weak(val, val + 1)) {}
#else
        {
            esp8266::InterruptLock lock;
            val = value.load();
            value.store(val + 1);
        }
#endif
        if (val++) return true;
        if (pendingTasks->available()) {
            pendingTasks->pop()->sleep(false);
#ifndef ESP8266
            while (!value.compare_exchange_weak(val, val - 1)) {}
#else
            {
                esp8266::InterruptLock lock;
                val = value.load();
                value.store(val - 1);
            }
#endif
        }
        return true;
    }
    // @returns: true if sucessfully aquired the semaphore, either immediately or after sleeping. false if maximum number of pending tasks is exceeded.
    bool wait()
    {
        unsigned val = value.load();
        if (val)
        {
            value.store(val - 1);
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
