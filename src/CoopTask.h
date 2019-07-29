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
    template< typename T >	T& move(T& t) noexcept { return t; }
}
#endif

#if !defined(ESP32) && !defined(ESP8266)
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#endif

class BasicCoopTask
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

#ifdef ARDUINO
    const String taskName;
#else
    const std::string taskName;
#endif

#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
    using taskfunction_t = std::function< void() noexcept >;
#else
    using taskfunction_t = void(*)() noexcept;
#endif

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
    bool delayed = false;
    std::atomic<bool> sleeps;

    static BasicCoopTask* current;

    bool initialize();
    void doYield(uint32_t val);

    void _exit();
    void _yield();
    void _sleep();
    void _delay(uint32_t ms);
    void _delayMicroseconds(uint32_t us);

private:
    taskfunction_t func;

public:
#ifdef ARDUINO
    BasicCoopTask(const String& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    BasicCoopTask(const std::string& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        taskName(name), taskStackSize(stackSize), sleeps(false), func(_func)
    {
    }
    BasicCoopTask(const BasicCoopTask&) = delete;
    BasicCoopTask& operator=(const BasicCoopTask&) = delete;
    ~BasicCoopTask()
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

    bool delayIsMs() const { return delay_ms; }

    void IRAM_ATTR sleep(const bool state) { sleeps.store(state); }

    // @returns: true if called from the task function of a CoopTask, false otherwise.
    static bool running() { return current; }

    // @returns: a reference to CoopTask instance that is running. Undefined if not called from a CoopTask function (running() == false).
    static BasicCoopTask& self() { return *current; }

    bool sleeping() const { return sleeps.load(); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return is to be preferred in most cases.
    static void exit() { self()._exit(); }
    /// use only in running CoopTask function.
    static void yield() { self()._yield(); }
    /// use only in running CoopTask function.
    static void sleep() { self()._sleep(); }
    /// use only in running CoopTask function.
    static void delay(uint32_t ms) { self()._delay(ms); }
    /// use only in running CoopTask function.
    static void delayMicroseconds(uint32_t us) { self()._delayMicroseconds(us); }
};

template<typename Result = int> class CoopTask : public BasicCoopTask
{
protected:
#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
    using taskfunction_t = std::function< Result() noexcept >;
#else
    using taskfunction_t = Result(*)() noexcept;
#endif

    Result _exitCode;

    static void captureFuncReturn()
    {
        self()._exitCode = self().func();
    }
    void _exit(Result&& code = Result())
    {
        _exitCode = std::move(code);
        BasicCoopTask::_exit();
    }
    void _exit(const Result& code)
    {
        _exitCode = code;
        BasicCoopTask::_exit();
    }

private:
    taskfunction_t func;

public:
#if defined(ARDUINO)
    CoopTask(const String& name, CoopTask::taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    CoopTask(const std::string& name, CoopTask::taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        // Wrap _func into _exit() to capture return value as exit code
        BasicCoopTask(name, captureFuncReturn, stackSize), func(_func)
    {
    }

    // @returns: The exit code is either the return value of of the task function, or set by using the exit() function.
    Result exitCode() const { return _exitCode; }

    // @returns: a reference to CoopTask instance that is running. Undefined if not called from a CoopTask function (running() == false).
    static CoopTask& self() { return static_cast<CoopTask&>(BasicCoopTask::self()); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return is to be preferred in most cases.
    // @param code default exit code is default value of CoopTask<>'s template argument, use exit() to set a different value.
    static void exit(Result&& code = Result()) { self()._exit(std::move(code)); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return is to be preferred in most cases.
    // @param code default exit code is default value of CoopTask<>'s template argument, use exit() to set a different value.
    static void exit(const Result& code) { self()._exit(code); }
};

#ifdef ESP8266
bool rescheduleTask(BasicCoopTask* task, uint32_t repeat_us);
#endif
bool IRAM_ATTR scheduleTask(BasicCoopTask* task, bool wakeup = false);

// temporary hack until delay() hook is available on platforms
#if defined(ESP32)
#define yield() { \
    if (BasicCoopTask::running()) BasicCoopTask::yield(); \
    else ::yield(); \
}
#define delay(m) { \
    if (BasicCoopTask::running()) BasicCoopTask::delay(m); \
    else ::delay(m); \
}
#endif
#ifndef ARDUINO
inline void yield() { BasicCoopTask::yield(); }
inline void delay(uint32_t ms) { BasicCoopTask::delay(ms); }
#endif

#endif // __CoopTask_h
