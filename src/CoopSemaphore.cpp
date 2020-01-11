/*
CoopSemaphore.cpp - Implementation of a semaphore for cooperative scheduling tasks
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

#include "CoopSemaphore.h"

#if defined(ESP8266)
#include <interrupts.h>
using esp8266::InterruptLock;
#elif defined(ESP32) || !defined(ARDUINO)
using std::min;
#else
class InterruptLock {
public:
    InterruptLock() {
        noInterrupts();
    }
    ~InterruptLock() {
        interrupts();
    }
};
#endif

#ifndef ARDUINO
#include <chrono>
namespace
{
    uint32_t millis()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}
#endif

bool CoopSemaphore::_wait(const bool withDeadline, const uint32_t ms)
{
    const uint32_t start = withDeadline ? millis() : 0;
    uint32_t expired = 0;
    bool selfFirst = false;
    for (;;)
    {
        auto self = CoopTaskBase::self();
        unsigned val;
#if !defined(ESP32) && defined(ARDUINO)
        {
            InterruptLock lock;
            val = value.load();
            if (val)
            {
                value.store(val - 1);
            }
        }
#else
        val = 1;
        while (val && !value.compare_exchange_weak(val, val - 1)) {}
#endif
        const unsigned valOnEntry = val;
        if (withDeadline) expired = millis() - start;
        if (!(selfFirst && valOnEntry))
        {
            if (pendingTasks.push(self))
            {
                if (!withDeadline) self->sleep(true);
            }
            else
            {
                selfFirst = true;
            }
        }
        bool fwd = !selfFirst && val;
        bool stop = false;
        CoopTaskBase* pendingTask = nullptr;
        bool selfSuccess = false;
        for (;;)
        {
            if (pendingTasks.available())
            {
#if !defined(ESP32) && defined(ARDUINO)
                {
                    InterruptLock lock;
                    pendingTask = pendingTask0.load();
                    if (fwd || !pendingTask) pendingTask0.store(pendingTasks.pop());
                }
#else
                pendingTask = nullptr;
                bool exchd = false;
                while ((fwd || !pendingTask) && !(exchd = pendingTask0.compare_exchange_weak(pendingTask, pendingTasks.peek()))) {}
                if (exchd) pendingTasks.pop();
#endif
            }
            else
            {
#if !defined(ESP32) && defined(ARDUINO)
                {
                    InterruptLock lock;
                    pendingTask = pendingTask0.load();
                    if (fwd && pendingTask) pendingTask0.store(nullptr);
                }
#else
                pendingTask = nullptr;
                if (fwd) pendingTask = pendingTask0.exchange(nullptr);
#endif
                stop = true;
            }
            if (!val)
            {
                break;
            }
            if (!(pendingTask || stop))
            {
                continue;
            }
            if (selfFirst)
            {
                if (!withDeadline) self->sleep(false);
                selfFirst = false;
                selfSuccess = true;
            }
            else if (pendingTask == self)
            {
                if (selfSuccess)
                {
                    if (!stop) continue;
                }
                else
                {
                    if (!withDeadline) self->sleep(false);
                    return true;
                }
            }
            else if (pendingTask)
            {
                pendingTask->scheduleTask(true);
            }
            if (stop)
            {
                break;
            }
            val -= 1;
            fwd = val;
        }
        if (selfSuccess)
        {
            return true;
        }
        if (valOnEntry)
        {
#if !defined(ESP32) && defined(ARDUINO)
            InterruptLock lock;
            val = value.load();
            value.store(val + 1);
#else
            while (!value.compare_exchange_weak(val, val + 1)) {}
#endif
        }
        if (withDeadline)
        {
            if (expired >= ms)
            {
                pendingTasks.for_each_rev_requeue(notIsSelfTask);
#if !defined(ESP32) && defined(ARDUINO)
                {
                    InterruptLock lock;
                    pendingTask = pendingTask0.load();
                    if (pendingTask == self) pendingTask0.store(pendingTasks.available() ? pendingTasks.pop() : nullptr);
                }
#else
                bool exchd = false;
                pendingTask = self;
                while ((pendingTask == self) && !(exchd = pendingTask0.compare_exchange_weak(pendingTask, pendingTasks.available() ? pendingTasks.peek() : nullptr))) {}
                if (exchd && pendingTasks.available()) pendingTasks.pop();
#endif
                return false;
            }
            CoopTaskBase::delay(ms - expired);
        }
        else
        {
            CoopTaskBase::yield();
        }
        selfFirst = true;
    }
}

bool IRAM_ATTR CoopSemaphore::post()
{
    CoopTaskBase* pendingTask;
#if !defined(ESP32) && defined(ARDUINO)
    {
        InterruptLock lock;
        unsigned val = value.load();
        value.store(val + 1);
        pendingTask = pendingTask0.load();
        if (pendingTask) pendingTask0.store(nullptr);
    }
#else
    unsigned val = 0;
    while (!value.compare_exchange_weak(val, val + 1)) {}
    pendingTask = pendingTask0.exchange(nullptr);
#endif
    if (pendingTask)
    {
        pendingTask->scheduleTask(true);
    }
    return true;
}

bool CoopSemaphore::setval(unsigned newVal)
{
    CoopTaskBase* pendingTask = nullptr;
    unsigned val;
#if !defined(ESP32) && defined(ARDUINO)
    {
        InterruptLock lock;
        val = value.load();
        value.store(newVal);
        if (newVal > val)
        {
            pendingTask = pendingTask0.load();
            pendingTask0.store(nullptr);
        }
    }
#else
    val = value.exchange(newVal);
    if (newVal > val) pendingTask = pendingTask0.exchange(nullptr);
#endif
    if (!pendingTask || !pendingTask->suspended()) return true;
    return pendingTask->scheduleTask(true);
}

bool CoopSemaphore::try_wait()
{
    unsigned val;
#if !defined(ESP32) && defined(ARDUINO)
    {
        InterruptLock lock;
        val = value.load();
        if (val)
        {
            value.store(val - 1);
        }
    }
#else
    val = 1;
    while (val && !value.compare_exchange_weak(val, val - 1)) {}
#endif
    return val;
}
