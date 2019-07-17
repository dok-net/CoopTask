/*
CoopTask.cpp - Implementation of cooperative scheduling tasks
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

#include "CoopTask.h"
#include <alloca.h>

// Integration into global yield() and delay()
#if defined(ESP8266) /* || defined(ESP32) - temporarily disabled until delay() hook is available on platforms */
extern "C" {
    void __yield();

    void yield()
    {
        if (CoopTask::running()) CoopTask::yield();
        else __yield();
    }
}
#elif !defined(ESP32) // Arduino
extern "C" {
    void yield()
    {
        if (CoopTask::running()) CoopTask::yield();
    }}
#endif
/* - temporarily disabled until delay() hook is available on platforms
#if defined(ESP32)
extern "C" {
    extern void __delay(uint32_t ms);

    void delay(uint32_t ms)
    {
        if (CoopTask::running()) CoopTask::delay(ms);
        else __delay(ms);
    }
}
#elif defined(ESP8266)
extern "C" {
    extern void __delay(unsigned long ms);

    void delay(unsigned long ms)
    {
        if (CoopTask::running()) CoopTask::delay(ms);
        else __delay(ms);
    }
}
#endif
*/

CoopTask* CoopTask::current = nullptr;

CoopTask::operator bool()
{
    if (!cont) return false;
    if (taskStackTop) return true;
    if (taskStackSize <= MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE))
    {
#if defined(ESP8266) || defined(ESP32)
        taskStackTop = new (std::nothrow) char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
#else
        taskStackTop = new char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
#endif
    }
    return taskStackTop;
}

bool CoopTask::initialize()
{
    if (!cont || init) return false;
    init = true;
    if (*this)
    {
        // fill stack with magic values to check overflow, corruption, and high water mark
        for (uint32_t pos = 0; pos <= (taskStackSize + sizeof(STACKCOOKIE)) / sizeof(uint32_t); ++pos)
        {
            reinterpret_cast<uint32_t*>(taskStackTop)[pos] = STACKCOOKIE;
        }
        char* bp = static_cast<char*>(alloca(reinterpret_cast<char*>(&bp) - (taskStackTop + taskStackSize + sizeof(STACKCOOKIE))));
        //Serial.printf("CoopTask %s: bp = %p, taskStackTop = %p, taskStackTop + taskStackSize + sizeof(STACKCOOKIE) = %p\n", taskName.c_str(), bp, taskStackTop, taskStackTop + taskStackSize + sizeof(STACKCOOKIE));
        _exit(func());
    }
    cont = false;
    return false;
}

uint32_t CoopTask::run()
{
    if (!cont) return 0;
    if (sleeps) return 1;
    if (delayed)
    {
        if (delay_ms)
        {
            int32_t delay_rem = static_cast<int32_t>(delay_exp - millis());
            if (delay_rem > 0) return delay_rem;
        }
        else
        {
            int32_t delay_rem = static_cast<int32_t>(delay_exp - micros());
            if (delay_rem >= DELAYMICROS_THRESHOLD) return delay_rem;
            if (delay_rem > 0) ::delayMicroseconds(delay_rem);
        }
        delayed = false;
    }
    auto val = setjmp(env);
    // val = 0: init; 1: exit() task; 2: yield task; 3: sleep task; >3: delay task until target delay_exp
    if (!val) {
        current = this;
        if (!init) return initialize();
        if (*reinterpret_cast<uint32_t*>(taskStackTop + taskStackSize + sizeof(STACKCOOKIE)) != STACKCOOKIE)
        {
            printf("FATAL ERROR: CoopTask %s stack corrupted\n", name().c_str());
            ::abort();
        }

        longjmp(env_yield, 0);
    }
    else
    {
        current = nullptr;
        if (*reinterpret_cast<uint32_t*>(taskStackTop) != STACKCOOKIE)
        {
            printf("FATAL ERROR: CoopTask %s stack overflow\n", name().c_str());
            ::abort();
        }
        cont &= val > 1;
        sleeps = val == 3;
        delayed = val > 3;
    }
    if (!cont) {
        delete[] taskStackTop;
        taskStackTop = nullptr;
        return 0;
    }
    switch (val)
    {
    case 2:
    case 3:
        return 1;
        break;
    default:
        return delay_exp > 2 ? delay_exp : 2;
        break;
    }
}

uint32_t CoopTask::getFreeStack()
{
    if (!taskStackTop) return 0;
    uint32_t pos;
    for (pos = 1; pos < (taskStackSize + sizeof(STACKCOOKIE)) / sizeof(uint32_t); ++pos)
    {
        if (STACKCOOKIE != reinterpret_cast<uint32_t*>(taskStackTop)[pos])
            break;
    }
    return (pos - 1) * sizeof(uint32_t);
}

void CoopTask::doYield(uint32_t val)
{
    if (!setjmp(env_yield))
    {
        longjmp(env, val);
    }
}

void CoopTask::_exit(int code)
{
    _exitCode = code;
    longjmp(env, 1);
}

void CoopTask::_yield()
{
    doYield(2);
}

void CoopTask::_sleep()
{
    doYield(3);
}

void CoopTask::_delay(uint32_t ms)
{
    delay_ms = true;
    delay_exp = millis() + ms;
    // CoopTask::run() defers task until delay_exp is reached
    doYield(4);
}

void CoopTask::_delayMicroseconds(uint32_t us)
{
    if (us < DELAYMICROS_THRESHOLD) {
        ::delayMicroseconds(us);
        return;
    }
    delay_ms = false;
    delay_exp = micros() + us;
    // CoopTask::run() defers task until delay_exp is reached
    doYield(4);
}
