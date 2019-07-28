/*
CoopTask.h - Implementation of cooperative scheduling tasks
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

#ifndef __CoopTask_h
#define __CoopTask_h

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
}
#endif

#if !defined(ESP32) && !defined(ESP8266)
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#endif

class CoopTask
{
protected:
    static constexpr uint32_t STACKCOOKIE = 0xdeadbeef;
#if defined(ESP32)
    static constexpr uint32_t MAXSTACKSPACE = 0x2000;
#elif defined(ESP8266)
    static constexpr uint32_t MAXSTACKSPACE = 0x1000;
#elif defined(ARDUINO)
    static constexpr uint32_t MAXSTACKSPACE = 0x180;
#else
    static constexpr uint32_t MAXSTACKSPACE = 0x10000;
#endif
    static constexpr uint32_t DEFAULTTASKSTACKSIZE = MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE);
    static constexpr int32_t DELAYMICROS_THRESHOLD = 50;

#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
    using taskfunction_t = std::function< int() noexcept >;
#else
    using taskfunction_t = int(*)() noexcept;
#endif

#ifdef ARDUINO
    const String taskName;
#else
    const std::string taskName;
#endif
    taskfunction_t func;
    uint32_t taskStackSize;
#ifndef _MSC_VER
    char* taskStackTop = nullptr;
    jmp_buf env;
    jmp_buf env_yield;
#else
    static LPVOID primaryFiber;
    LPVOID taskFiber;
    int val;
    static void __stdcall taskFiberFunc(void*);
#endif
    // true: delay_exp is vs. millis(); false: delay_exp is vs. micros().
    bool delay_ms = false;
    uint32_t delay_exp = 0;
    bool init = false;
    bool cont = true;
    int _exitCode = 0;
    bool delayed = false;
    std::atomic<bool> sleeps;

    static CoopTask* current;

    bool initialize();
    void doYield(uint32_t val);

    void _exit(int code = 0);
    void _yield();
    void _sleep();
    void _delay(uint32_t ms);
    void _delayMicroseconds(uint32_t us);

public:
#ifdef ARDUINO
    CoopTask(const String& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    CoopTask(const std::string& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        taskName(name), func(_func), taskStackSize(stackSize), sleeps(false)
    {
    }
    CoopTask(const CoopTask&) = delete;
    CoopTask& operator=(const CoopTask&) = delete;
    ~CoopTask()
    {
#ifndef _MSC_VER
        delete[] taskStackTop;
#endif
    }

#ifdef ARDUINO
    const String& name() const { return taskName; }
#else
    const std::string& name() const { return taskName; }
#endif

    // @returns: true if the CoopTask object is ready to run, including stack allocation.
    //           false if either initialization has failed, or the task has exited().
    operator bool();
    // @returns: 0: exited. 1: runnable or sleeping. >1: delayed until millis() or micros() deadline, check delayIsMs().
    uint32_t run();

    // @returns: size of unused stack space. 0 if stack is not allocated yet or was deleted after task exited.
    uint32_t getFreeStack();

    // @returns: default exit code is 0, using exit() the task can set a different value.
    int exitCode() const { return _exitCode; }

    bool delayIsMs() const { return delay_ms; }

    void IRAM_ATTR sleep(const bool state) { sleeps.store(state); }

    // @returns: true if called from the task function of a CoopTask, false otherwise.
    static bool running() { return current; }

    // @returns: a reference to CoopTask instance that is running. Undefined if not called from a CoopTask function (running() == false).
    static CoopTask& self() { return *current; }

    bool sleeping() const { return sleeps.load(); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return is to be preferred in most cases.
    // @param code default exit code is 0, use exit() to set a different value.
    static void exit(int code = 0) { current->_exit(code); }
    /// use only in running CoopTask function.
    static void yield() { current->_yield(); }
    /// use only in running CoopTask function.
    static void sleep() { current->_sleep(); }
    /// use only in running CoopTask function.
    static void delay(uint32_t ms) { current->_delay(ms); }
    /// use only in running CoopTask function.
    static void delayMicroseconds(uint32_t us) { current->_delayMicroseconds(us); }
};

#ifdef ESP8266
bool rescheduleTask(CoopTask* task, uint32_t repeat_us);
#endif
bool IRAM_ATTR scheduleTask(CoopTask* task, bool wakeup = false);

// temporary hack until delay() hook is available on platforms
#if defined(ESP32)
#define yield() { \
    if (CoopTask::running()) CoopTask::yield(); \
    else ::yield(); \
}
#define delay(m) { \
    if (CoopTask::running()) CoopTask::delay(m); \
    else ::delay(m); \
}
#endif
#ifndef ARDUINO
inline void yield() { CoopTask::yield(); }
inline void delay(uint32_t ms) { CoopTask::delay(ms); }
#endif

#endif // __CoopTask_h
