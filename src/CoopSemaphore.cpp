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
    const uint32_t start = millis();
    uint32_t expired = 0;
    for (;;)
    {
        auto& self = CoopTaskBase::self();
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
        if (!val)
        {
            expired = millis() - start;
            if (withDeadline && expired >= ms) return false;
            if (!pendingTasks->push(&self)) return false;
        }
        CoopTaskBase* pendingTask = nullptr;
        const bool forcePop = val > 1;
        for (;;)
        {
            if (pendingTasks->available())
            {
#if !defined(ESP32) && defined(ARDUINO)
                InterruptLock lock;
                if (!(pendingTask = pendingTask0.load()) || forcePop)
                {
                    pendingTask0.store(pendingTasks->pop());
                }
#else
                bool exchd = false;
                while ((!pendingTask || forcePop) && !(exchd = pendingTask0.compare_exchange_strong(pendingTask, pendingTasks->peek()))) {}
                if (exchd) pendingTasks->pop();
#endif
            }
            else
            {
#if !defined(ESP32) && defined(ARDUINO)
                InterruptLock lock;
                pendingTask = pendingTask0.load();
                pendingTask0.store(nullptr);
#else
                while (!pendingTask0.compare_exchange_weak(pendingTask, nullptr)) {}
#endif
                if (val) val = 1;
            }
            if (val <= 1) break;
            if (!pendingTask)
            {
                continue;
            }
            val -= 1;

            if (pendingTask != &self && pendingTask->suspended())
            {
                if (pendingTask->delayed().load()) { pendingTask->sleep(false); }
                else { scheduleTask(pendingTask, true); }
            }
        }
        if (val) return true;
        if (withDeadline)
        {

            delay(expired >= ms ? 0 : ms - expired);
#if !defined(ESP32) && defined(ARDUINO)
            InterruptLock lock;
            pendingTask = pendingTask0.load();
            if (pendingTask == &self) pendingTask0.store(nullptr);
#else
            if (pendingTask == &self && !pendingTask0.compare_exchange_weak(pendingTask, nullptr)) {}
#endif
            if (pendingTask != &self) pendingTasks->for_each_rev_requeue([](CoopTaskBase*& pendingTask)
                {
                    return pendingTask != &CoopTaskBase::self();
                });
        }
        else
        {
            CoopTaskBase::sleep();
        }
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
        pendingTask0.store(nullptr);
    }
#else
    unsigned val = 0;
    while (!value.compare_exchange_weak(val, val + 1)) {}
    pendingTask = pendingTask0.exchange(nullptr);
#endif
    if (!pendingTask || !pendingTask->suspended()) return true;
    if (pendingTask->delayed().load())
    {
        pendingTask->sleep(false);
        return true;
    }
    return scheduleTask(pendingTask, true);
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
    if (pendingTask->delayed().load())
    {
        pendingTask->sleep(false);
        return true;
    }
    return scheduleTask(pendingTask, true);
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
