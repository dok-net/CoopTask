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

#if defined(ESP8266)
#include <atomic>
#elif defined(ESP32) || !defined(ARDUINO)
#include <atomic>
#else
#include <util/atomic.h>

namespace std
{
    template< typename T > class unique_ptr
    {
    public:
        using pointer = T *;
        unique_ptr() noexcept : ptr(nullptr) {}
        unique_ptr(pointer p) : ptr(p) {}
        pointer operator->() const noexcept { return ptr; }
        T& operator[](size_t i) const { return ptr[i]; }
        void reset(pointer p = pointer()) noexcept
        {
            delete ptr;
            ptr = p;
        }
        T& operator*() const { return *ptr; }
    private:
        pointer ptr;
    };

    typedef enum memory_order {
        memory_order_relaxed,
        memory_order_acquire,
        memory_order_release,
        memory_order_seq_cst
    } memory_order;
    template< typename T > class atomic {
    private:
        T value;
    public:
        atomic() {}
        atomic(T desired) { value = desired; }
        void store(T desired, std::memory_order = std::memory_order_seq_cst) volatile noexcept { value = desired; }
        T load(std::memory_order = std::memory_order_seq_cst) const volatile noexcept { return value; }
    };
    template< typename T >	T&& move(T& t) noexcept { return static_cast<T&&>(t); }
}
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
#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
    using taskfunction_t = std::function< void() noexcept >;
#else
    using taskfunction_t = void(*)() noexcept;
#endif

#ifdef ARDUINO
    CoopTaskBase(const String& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    CoopTaskBase(const std::string& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        taskName(name), taskStackSize(stackSize), delays(false), sleeps(false), func(_func)
    {
    }
    CoopTaskBase(const CoopTaskBase&) = delete;
    CoopTaskBase& operator=(const CoopTaskBase&) = delete;
    ~CoopTaskBase()
    {
#ifdef _MSC_VER
        if (taskFiber) DeleteFiber(taskFiber);
#endif
    }

    static constexpr int32_t DELAYMICROS_THRESHOLD = 50;

#ifdef ARDUINO
    const String taskName;
#else
    const std::string taskName;
#endif

    uint32_t taskStackSize;
#ifndef _MSC_VER
    char* taskStackTop = nullptr;
    jmp_buf env;
    jmp_buf env_yield;
#else
    static LPVOID primaryFiber;
    LPVOID taskFiber = nullptr;
    int val = 0;
    static void __stdcall taskFiberFunc(void*);
#endif
    // true: delay_start/delay_duration are in milliseconds; false: delay_start/delay_duration are in microseconds.
    bool delay_ms = false;
    uint32_t delay_start = 0;
    uint32_t delay_duration = 0;
    bool init = false;
    bool cont = true;
    std::atomic<bool> delays;
    std::atomic<bool> sleeps;

    static CoopTaskBase* current;

    int32_t initialize();
    void doYield(uint32_t val) noexcept;

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
#ifndef _MSC_VER
    operator bool() noexcept { return cont && taskStackTop; }
#else
    operator bool() noexcept { return cont; }
#endif

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

    /// @returns: true if called from the task function of a CoopTask, false otherwise.
    static bool running() noexcept { return current; }

    /// @returns: a reference to CoopTask instance that is running. Undefined if not called from a CoopTask function (running() == false).
    static CoopTaskBase& self() noexcept { return *current; }

    inline bool IRAM_ATTR sleeping() const noexcept __attribute__((always_inline)) { return sleeps.load(); }
    inline const std::atomic<bool>& IRAM_ATTR delayed() const noexcept __attribute__((always_inline)) { return delays; }
    inline bool IRAM_ATTR suspended() const noexcept __attribute__((always_inline)) { return sleeps.load() || delays.load(); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return or exceptions is to be preferred in most cases.
    static void exit() noexcept { self()._exit(); }
    /// use only in running CoopTask function.
    static void yield() noexcept { self()._yield(); }
    /// use only in running CoopTask function.
    static void sleep() noexcept { self()._sleep(); }
    /// use only in running CoopTask function.
    static void delay(uint32_t ms) noexcept { self()._delay(ms); }
    /// use only in running CoopTask function.
    static void delayMicroseconds(uint32_t us) noexcept { self()._delayMicroseconds(us); }
};

#if defined(ESP8266) // TODO: requires some PR to be merged: || defined(ESP32)
bool rescheduleTask(CoopTaskBase* task, uint32_t repeat_us);
#endif

/// Add the task to the scheduler, optionally waking up the task first.
/// @returns: true on success.
bool IRAM_ATTR scheduleTask(CoopTaskBase* task, bool wakeup = false);

// TODO: temporary hack until delay() hook is available on ESP32
#if defined(ESP32)
#define yield() { \
    if (CoopTaskBase::running()) CoopTaskBase::yield(); \
    else ::yield(); \
}
#define delay(m) { \
    if (CoopTaskBase::running()) CoopTaskBase::delay(m); \
    else ::delay(m); \
}
#endif

#ifndef ARDUINO
inline void yield() { CoopTaskBase::yield(); }
inline void delay(uint32_t ms) { CoopTaskBase::delay(ms); }
#endif

#endif // __CoopTaskBase_h
