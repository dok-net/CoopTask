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
#ifdef ESP8266
#include <interrupts.h>
#endif
#if !defined(ESP8266) && !defined(ESP32) && defined(ARDUINO)
#include <util/atomic.h>

namespace esp8266
{
    class InterruptLock {
    public:
        InterruptLock() {
            noInterrupts();
        }
        ~InterruptLock() {
            interrupts();
        }
    };
}
namespace std
{
    typedef enum memory_order {
        //	memory_order_relaxed,
        //	//memory_order_consume,
        //	memory_order_acquire,
        //	memory_order_release,
        //	//memory_order_acq_rel,
        memory_order_seq_cst
    } memory_order;
    template< typename T > class atomic {
    private:
        T value;
    public:
        atomic(T desired) { value = desired; }
        void store(T desired, std::memory_order = std::memory_order_seq_cst) volatile noexcept { value = desired; }
        T load(std::memory_order = std::memory_order_seq_cst) const volatile noexcept { return value; }
    };
    //extern "C" void atomic_thread_fence(std::memory_order) noexcept {}
    //template< typename T >	T& move(T& t) noexcept { return t; }
}

#else
#include "circular_queue/circular_queue.h"
#include <atomic>
#endif

#if !defined(ESP32) && !defined(ESP8266)
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#endif

/// A semaphore that is safe to use from CoopTasks.
/// Only post() is safe to use from interrupt service routines,
/// or concurrent OS threads that must synchronized with the singled thread running CoopTasks.
class CoopSemaphore
{
protected:
    std::atomic<unsigned> value;
#if !defined(ESP8266) && !defined(ESP32) && defined(ARDUINO)
    CoopTask** pendingTasks;
#else
    std::unique_ptr<circular_queue<CoopTask*>> pendingTasks;
#endif

public:
    /// @param val the initial value of the semaphore.
    /// @param maxPending the maximum supported number of concurrently waiting tasks.
#if !defined(ESP8266) && !defined(ESP32) && defined(ARDUINO)
    CoopSemaphore(unsigned val, unsigned maxPending = 10) : value(val), pendingTasks(new CoopTask* [maxPending]) {}
#else
    CoopSemaphore(unsigned val, unsigned maxPending = 10) : value(val), pendingTasks(new circular_queue<CoopTask*>(maxPending)) {}
#endif
    CoopSemaphore(const CoopSemaphore&) = delete;
    CoopSemaphore& operator=(const CoopSemaphore&) = delete;
    ~CoopSemaphore()
    {
        // wake up all queued tasks
#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
        pendingTasks->for_each([](CoopTask* task) { task->sleep(false); });
#endif
    }

    /// post() is the only operation that is allowed from an interrupt service routine,
    /// or a concurrent OS thread that is synchronized with the singled thread running CoopTasks.
    bool IRAM_ATTR post()
    {
#if !defined(ESP32) && defined(ARDUINO)
        {
            esp8266::InterruptLock lock;
            unsigned val = value.load();
            value.store(val + 1);
            if (val) {
                return true;
            }
        }
#else
        unsigned val = 0;
        while (!value.compare_exchange_weak(val, val + 1)) {}
        if (val) return true;
#endif
        if (pendingTasks->available()) {
            pendingTasks->pop()->sleep(false);
#if !defined(ESP32) && defined(ARDUINO)
            {
                esp8266::InterruptLock lock;
                value.store(value.load() - 1);
            }
#else
            unsigned val = 1;
            while (!value.compare_exchange_weak(val, val - 1)) {}
#endif
        }
        return true;
    }

    // @returns: true if sucessfully aquired the semaphore, either immediately or after sleeping. false if maximum number of pending tasks is exceeded.
    bool wait()
    {
#if !defined(ESP32) && defined(ARDUINO)
        {
            esp8266::InterruptLock lock;
            unsigned val = value.load();
            if (val)
            {
                value.store(val - 1);
                return true;
            }
            if (!pendingTasks->push(&CoopTask::self())) return false;
            CoopTask::self().sleep(true);
        }
        CoopTask::sleep();
#else
        unsigned val = value.load();
        while (val && !value.compare_exchange_weak(val, val - 1)) {}
        if (!val)
        {
            if (!pendingTasks->push(&CoopTask::self())) return false;
            CoopTask::sleep();
        }
#endif
        return true;
    }

    bool try_wait()
    {
#if !defined(ESP32) && defined(ARDUINO)
        {
            esp8266::InterruptLock lock;
            unsigned val = value.load();
            if (!val) return false;
            value.store(val - 1);
        }
        return true;
#else
        unsigned val = 1;
        while (val && !value.compare_exchange_weak(val, val - 1)) {}
        return val > 0;
#endif
    }
};

#endif // __CoopSemaphore_h
