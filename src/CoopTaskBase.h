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

#ifdef ESP32
#define ESP32_FREERTOS
#endif

#include "circular_queue/Delegate.h"
#if defined(ESP8266) || defined(ESP32)
#include <array>
#include <memory>
#include <csetjmp>
#include <Arduino.h>
#elif defined(ARDUINO)
#include <setjmp.h>
#include <Arduino.h>
#elif defined(_MSC_VER)
#include <array>
#include <Windows.h>
#include <string>
#else
#include <array>
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
public:
    static constexpr bool FULLFEATURES = sizeof(unsigned) >= 4;

protected:
    using taskfunction_t = Delegate< void() noexcept >;

#ifdef ARDUINO
    CoopTaskBase(const String& name, taskfunction_t _func, unsigned stackSize = DEFAULTTASKSTACKSIZE) :
#else
    CoopTaskBase(const std::string& name, taskfunction_t _func, unsigned stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        taskName(name), taskStackSize(stackSize), sleeps(true), delays(false), func(_func)
    {
    }
    CoopTaskBase(const CoopTaskBase&) = delete;
    CoopTaskBase& operator=(const CoopTaskBase&) = delete;

    static constexpr int32_t DELAYMICROS_THRESHOLD = 50;
    static constexpr uint32_t DELAY_MAXINT = (~(uint32_t)0) >> 1;

#ifdef ARDUINO
    const String taskName;
#else
    const std::string taskName;
#endif

    unsigned taskStackSize;
#if defined(_MSC_VER)
    static LPVOID primaryFiber;
    LPVOID taskFiber = nullptr;
    int val = 0;
    static void __stdcall taskFiberFunc(void* self);
#elif defined(ESP32_FREERTOS)
    TaskHandle_t taskHandle = nullptr;
    static void taskFunc(void* _self);
#else
    char* taskStackTop = nullptr;
    static jmp_buf env;
    jmp_buf env_yield;
#endif
    static constexpr unsigned MAXNUMBERCOOPTASKS = FULLFEATURES ? 32 : 8;
    static std::array< std::atomic<CoopTaskBase* >, MAXNUMBERCOOPTASKS> runnableTasks;
    static std::atomic<unsigned> runnableTasksCount;
    static CoopTaskBase* current;
    bool init = false;
    bool cont = true;
    std::atomic<bool> sleeps;
    // ESP32 FreeRTOS (#define ESP32_FREERTOS) handles delays, on this platfrom delays is always false
    std::atomic<bool> delays;

    int32_t initialize();
    void doYield(unsigned val) noexcept;

#if defined(ESP8266)
    static bool usingBuiltinScheduler;
    bool rescheduleTask(uint32_t repeat_us);
#endif
    bool enrollRunnable();
    void delistRunnable();

    void _exit() noexcept;
    void _yield() noexcept;
    void _sleep() noexcept;
    void _delay(uint32_t ms) noexcept;
    void _delayMicroseconds(uint32_t us) noexcept;

private:
    // true: delay_start/delay_duration are in milliseconds; false: delay_start/delay_duration are in microseconds.
    bool delay_ms = false;
    uint32_t delay_start = 0;
    uint32_t delay_duration = 0;

    taskfunction_t func;

public:
    virtual ~CoopTaskBase();
#if defined(ESP32)
    static constexpr unsigned MAXSTACKSPACE = 0x2000;
#elif defined(ESP8266)
    static constexpr unsigned MAXSTACKSPACE = 0x1000;
#elif defined(ARDUINO)
    static constexpr unsigned MAXSTACKSPACE = FULLFEATURES ? 0x180 : 0xc0;
#else
    static constexpr unsigned MAXSTACKSPACE = 0x10000;
#endif
    static constexpr unsigned STACKCOOKIE = FULLFEATURES ? 0xdeadbeefUL : 0xdeadU;
    static constexpr unsigned DEFAULTTASKSTACKSIZE = MAXSTACKSPACE - (FULLFEATURES ? 2 : 1) * sizeof(STACKCOOKIE);

#ifdef ARDUINO
    const String& name() const noexcept { return taskName; }
#else
    const std::string& name() const noexcept { return taskName; }
#endif

    /// @returns: true if the CoopTask object is ready to run, including stack allocation.
    ///           false if either initialization has failed, or the task has exited().
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
    operator bool() noexcept { return cont && taskStackTop; }
#else
    operator bool() noexcept { return cont; }
#endif

    /// Ready the task for scheduling, by default waking up the task from both sleep and delay.
    /// @returns: true on success.
    bool IRAM_ATTR scheduleTask(bool wakeup = true);

#ifdef ESP8266
    /// For full access to all features, cyclic task scheduling, state evaluation
    /// and running are performed explicitly from user code. For convenience, the function
    /// runCoopTasks() implements the pattern as best practice. See the CoopTask examples for this.
    /// If only a pre-determined number of loop tasks need to run indefinitely
    /// without exit codes or explicit deep sleep system states, on platforms where a
    /// scheduler exists that is suffiently capable to iteratively run each of these CoopTasks,
    /// calling this function switches all task creation and scheduling to using that, obviating
    /// the need to call a scheduler explicitly from user code.
    /// The scheduler selection should be done before the first CoopTask is created, and not
    /// changed thereafter during runtime.
    /// By default, builtin schedulers are not used, for well-defined behavior and portability.
    /// @param state true: The parameter default value. All subsequent scheduling of tasks is
    /// handed to the builtin scheduler.
    static void useBuiltinScheduler(bool state = true)
    {
        usingBuiltinScheduler = state;
    }
#endif
    /// Every task is entered into this list by scheduleTask(). It is removed when it exits
    /// or gets deleted.
    static const std::array< std::atomic<CoopTaskBase* >, MAXNUMBERCOOPTASKS>& getRunnableTasks()
    {
        return runnableTasks;
    }
    /// @returns: the count of runnable, non-nullptr, tasks in the return of getRunnableTasks().
    static unsigned getRunnableTasksCount()
    {
        return runnableTasksCount.load();
    }

    /// @returns: -1: exited. 0: runnable or sleeping. >0: delayed for milliseconds or microseconds, check delayIsMs().
    int32_t run();

    /// @returns: size of unused stack space. 0 if stack is not allocated yet or was deleted after task exited.
    unsigned getFreeStack();

    bool delayIsMs() const noexcept { return delay_ms; }

    /// Modifies the sleep flag. if called from a running task, it is not immediately suspended.
    /// @param state true: a suspended task becomes sleeping, if call from the running task,
    /// the next call to yield() or delay() puts it into sleeping state.
    /// false: clears the sleeping and delay state of the task.
    void IRAM_ATTR sleep(const bool state) noexcept;

#ifdef ESP32_FREERTOS
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
    inline bool IRAM_ATTR delayed() const noexcept __attribute__((always_inline)) { return delays.load(); }
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

/// An optional convenience funtion that does all the work to cyclically perform CoopTask execution.
/// @param reaper An optional function that is called once when a task exits.
/// @param onDelay An optional function to handle a global delay greater or equal 1 millisecond, resulting
/// from the minimum time interval for which at this time all CoopTasks are delayed.
/// This can be used for power saving, if wake up by asynchronous events is properly considered.
/// onDelay() returns a bool value, if true, runCoopTasks performs the default housekeeping actions
/// in addition, otherwise it skips those.
void runCoopTasks(const Delegate<void(const CoopTaskBase* const task)>& reaper = nullptr, const Delegate<bool(uint32_t ms)>& onDelay = nullptr);

#endif // __CoopTaskBase_h
