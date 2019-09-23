/*
CoopTaskBase.h - Implementation of cooperative scheduling tasks
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

#ifndef __CoopTaskBase_h
#define __CoopTaskBase_h

#if defined(ESP8266) || defined(ESP32)
#include <functional>
#include <memory>
#include <csetjmp>
#include <Arduino.h>
#elif defined(ARDUINO)
#include <setjmp.h>
#include <Arduino.h>
#elif defined(_MSC_VER)
#include <functional>
#include <Windows.h>
#include <string>
#else
#include <functional>
#include <csetjmp>
#include <string>
#endif

#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
#include <atomic>
#else
#include "circular_queue/ghostl.h"
#endif

#if !defined(ESP32) && !defined(ESP8266)
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#endif

#ifdef _MSC_VER
#define __attribute__(_)
#endif

class CoopTaskBase
{
protected:
    using taskfunction_t = std::function< void() noexcept >;

#ifdef ARDUINO
    CoopTaskBase(const String& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    CoopTaskBase(const std::string& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        taskName(name), taskStackSize(stackSize), sleeps(true), delays(false), func(_func)
    {
    }
    CoopTaskBase(const CoopTaskBase&) = delete;
    CoopTaskBase& operator=(const CoopTaskBase&) = delete;
    ~CoopTaskBase()
    {
#if defined(_MSC_VER)
        if (taskFiber) DeleteFiber(taskFiber);
#elif defined(ESP32)
        if (taskHandle) vTaskDelete(taskHandle);
#endif
    }

    static constexpr int32_t DELAYMICROS_THRESHOLD = 50;

#ifdef ARDUINO
    const String taskName;
#else
    const std::string taskName;
#endif

    uint32_t taskStackSize;
#if defined(_MSC_VER)
    static LPVOID primaryFiber;
    LPVOID taskFiber = nullptr;
    int val = 0;
    static void __stdcall taskFiberFunc(void* self);
#elif defined(ESP32)
    TaskHandle_t taskHandle = nullptr;
    static void taskFunc(void* _self);
#else
    char* taskStackTop = nullptr;
    jmp_buf env;
    jmp_buf env_yield;
#endif
    static CoopTaskBase* current;
    bool init = false;
    bool cont = true;
    std::atomic<bool> sleeps;
    // ESP32 FreeRTOS handles delays, on this platfrom delays is always false
    std::atomic<bool> delays;
    // true: delay_start/delay_duration are in milliseconds; false: delay_start/delay_duration are in microseconds.
    bool delay_ms = false;
    uint32_t delay_start = 0;
    uint32_t delay_duration = 0;

    int32_t initialize();
    void doYield(uint32_t val) noexcept;

#if defined(ESP8266)
    bool rescheduleTask(uint32_t repeat_us);
#endif

    void _exit() noexcept;
    void _yield() noexcept;
    void _sleep() noexcept;
    void _delay(uint32_t ms) noexcept;
    void _delayMicroseconds(uint32_t us) noexcept;

private:
    taskfunction_t func;

public:
#if defined(ESP32)
    static constexpr uint32_t MAXSTACKSPACE = 0x2000;
#elif defined(ESP8266)
    static constexpr uint32_t MAXSTACKSPACE = 0x1000;
#elif defined(ARDUINO)
    static constexpr uint32_t MAXSTACKSPACE = 0x180;
#else
    static constexpr uint32_t MAXSTACKSPACE = 0x10000;
#endif
    static constexpr uint32_t STACKCOOKIE = 0xdeadbeef;
    static constexpr uint32_t DEFAULTTASKSTACKSIZE = MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE);

#ifdef ARDUINO
    const String& name() const noexcept { return taskName; }
#else
    const std::string& name() const noexcept { return taskName; }
#endif

    /// @returns: true if the CoopTask object is ready to run, including stack allocation.
    ///           false if either initialization has failed, or the task has exited().
#if !defined(_MSC_VER) && !defined(ESP32)
    operator bool() noexcept { return cont && taskStackTop; }
#else
    operator bool() noexcept { return cont; }
#endif

    /// Ready the task for scheduling, by default waking up the task from both sleep and delay.
    /// @returns: true on success.
    bool IRAM_ATTR scheduleTask(bool wakeup = true);

    /// @returns: -1: exited. 0: runnable or sleeping. >0: delayed for milliseconds or microseconds, check delayIsMs().
    int32_t run();

    /// @returns: size of unused stack space. 0 if stack is not allocated yet or was deleted after task exited.
    uint32_t getFreeStack();

    bool delayIsMs() const noexcept { return delay_ms; }

    /// Modifies the sleep flag. if called from a running task, it is not immediately suspended.
    /// @param state true: a suspended task becomes sleeping, if call from the running task,
    /// the next call to yield() or delay() puts it into sleeping state.
    /// false: clears the sleeping and delay state of the task.
    void IRAM_ATTR sleep(const bool state) noexcept;

#ifdef ESP32
    /// @returns: a pointer to the CoopTask instance that is running. nullptr if not called from a CoopTask function (running() == false).
    static CoopTaskBase* self() noexcept;
#else
    /// @returns: a pointer to the CoopTask instance that is running. nullptr if not called from a CoopTask function (running() == false).
    static CoopTaskBase* self() noexcept { return current; }
#endif
	/// @returns: true if called from the task function of a CoopTask, false otherwise.
	static bool running() noexcept { return self(); }

    /// @returns: true if the task's is set to sleep.
    /// For a non-running task, this implies it is also currently not scheduled.
    inline bool IRAM_ATTR sleeping() const noexcept __attribute__((always_inline)) { return sleeps.load(); }
    inline const std::atomic<bool>& IRAM_ATTR delayed() const noexcept __attribute__((always_inline)) { return delays; }
    inline bool IRAM_ATTR suspended() const noexcept __attribute__((always_inline)) { return sleeps.load() || delays.load(); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return or exceptions is to be preferred in most cases.
    static void exit() noexcept { self()->_exit(); }
    /// use only in running CoopTask function.
    static void yield() noexcept { self()->_yield(); }
    static void yield(CoopTaskBase* self) noexcept { self->_yield(); }
    /// use only in running CoopTask function.
    static void sleep() noexcept { self()->_sleep(); }
    /// use only in running CoopTask function.
    static void delay(uint32_t ms) noexcept { self()->_delay(ms); }
    static void delay(CoopTaskBase* self, uint32_t ms) noexcept { self->_delay(ms); }
    /// use only in running CoopTask function.
    static void delayMicroseconds(uint32_t us) noexcept { self()->_delayMicroseconds(us); }
};

#ifndef ARDUINO
inline void yield() { CoopTaskBase::yield(); }
inline void delay(uint32_t ms) { CoopTaskBase::delay(ms); }
#endif

#endif // __CoopTaskBase_h
