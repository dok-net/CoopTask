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
#ifdef ARDUINO
#include <alloca.h>
#else
#include <chrono>
#endif

#ifdef ESP8266
#include <Schedule.h>
#endif

extern "C" {
    // Integration into global yield() and delay()
#if defined(ESP8266) /* || defined(ESP32) - temporarily disabled until delay() hook is available on platforms */
    void __yield();

    void yield()
    {
        if (CoopTask::running()) CoopTask::yield();
        else __yield();
    }
#elif !defined(ESP32) && defined(ARDUINO)
    void yield()
    {
        if (CoopTask::running()) CoopTask::yield();
    }
#endif
#if defined(ESP8266)
    void __delay(unsigned long ms);

    void delay(unsigned long ms)
    {
        if (CoopTask::running()) CoopTask::delay(ms);
        else __delay(ms);
    }
/*
#elif defined(ESP32) - temporarily disabled until delay() hook is available on platforms
    void __delay(uint32_t ms);

    void delay(uint32_t ms)
    {
        if (CoopTask::running()) CoopTask::delay(ms);
        else __delay(ms);
    }
*/
#endif
}

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
#if defined(ARDUINO) || defined(__GNUC__)
        char* bp = static_cast<char*>(alloca(reinterpret_cast<char*>(&bp) - (taskStackTop + taskStackSize + sizeof(STACKCOOKIE))));
#elif defined(_MSC_VER) && defined (_WIN32) && !defined (_WIN64)
        char* stackBottom = taskStackTop + taskStackSize + sizeof(STACKCOOKIE);
        __asm mov esp, stackBottom;
#else
#error Setting stack pointer is not implemented on this target
#endif
        //Serial.printf("CoopTask %s: bp = %p, taskStackTop = %p, taskStackTop + taskStackSize + sizeof(STACKCOOKIE) = %p\n", taskName.c_str(), bp, taskStackTop, taskStackTop + taskStackSize + sizeof(STACKCOOKIE));
        CoopTask::exit(func());
    }
    cont = false;
    return false;
}

#ifndef ARDUINO
namespace
{
    uint32_t millis()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    uint32_t micros()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    void delayMicroseconds(uint32_t us)
    {
        const uint32_t start = micros() + us;
        while (micros() - start < us) {}
    }
}
#endif

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
        sleeps |= val == 3;
        delayed |= val > 3;
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

#ifdef ESP8266
bool rescheduleTask(CoopTask* task, uint32_t repeat_us)
{
    if (task->sleeping())
        return false;
    auto stat = task->run();
    switch (stat)
    {
    case 0: // exited.
        return false;
        break;
    case 1: // runnable or sleeping.
        if (task->sleeping()) return false;
        if (!repeat_us) return true;
        schedule_recurrent_function_us([task]() { return rescheduleTask(task, 0); }, 0);
        return false;
        break;
    default: // delayed until millis() or micros() deadline, check delayIsMs().
        if (task->sleeping()) return false;
        auto next_repeat_us = static_cast<int32_t>(task->delayIsMs() ? (stat - millis()) * 1000 : stat - micros());
        if (next_repeat_us < 0) next_repeat_us = 0;
        if (static_cast<uint32_t>(next_repeat_us) == repeat_us) return true;
        schedule_recurrent_function_us([task, next_repeat_us]() { return rescheduleTask(task, next_repeat_us); }, next_repeat_us);
        return false;
        break;
    }
    return false;
}
#endif

bool IRAM_ATTR scheduleTask(CoopTask* task, bool wakeup)
{
#ifdef ESP8266
    return schedule_function([task, wakeup]() {
        if (wakeup)
            task->sleep(false);
        if (rescheduleTask(task, 0)) {
            schedule_recurrent_function_us([task]() { return rescheduleTask(task, 0); }, 0);
        }
        });
#else
    task->sleep(!wakeup);
    return true;
#endif
}
