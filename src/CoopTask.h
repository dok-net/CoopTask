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

#include "BasicCoopTask.h"

template<typename Result = int, class StackAllocator = CoopTaskStackAllocator> class CoopTask : public BasicCoopTask<StackAllocator>
{
public:
    using taskfunction_t = Delegate< Result() >;

#if defined(ARDUINO)
    CoopTask(const String& name, CoopTask::taskfunction_t _func, size_t stackSize = BasicCoopTask<StackAllocator>::DEFAULTTASKSTACKSIZE) :
#else
    CoopTask(const std::string& name, CoopTask::taskfunction_t _func, size_t stackSize = BasicCoopTask<StackAllocator>::DEFAULTTASKSTACKSIZE) :
#endif
        // Wrap _func into _exit() to capture return value as exit code
        BasicCoopTask<StackAllocator>(name, captureFuncReturn, stackSize), func(_func)
    {
    }

protected:
    Result _exitCode = {};

    static void captureFuncReturn() noexcept
    {
#if !defined(ARDUINO)
        try {
#endif
            self()->_exitCode = self()->func();
#if !defined(ARDUINO)
        }
        catch (const Result code)
        {
            self()->_exitCode = code;
        }
        catch (...)
        {
        }
#endif
    }
    void _exit(Result&& code = Result{}) noexcept
    {
        _exitCode = std::move(code);
        BasicCoopTask<StackAllocator>::_exit();
    }
    void _exit(const Result& code) noexcept
    {
        _exitCode = code;
        BasicCoopTask<StackAllocator>::_exit();
    }

private:
    taskfunction_t func;

public:
    /// @returns: The exit code is either the return value of of the task function, or set by using the exit() function.
    Result exitCode() const noexcept { return _exitCode; }

    /// @returns: a pointer to the CoopTask instance that is running. nullptr if not called from a CoopTask function (running() == false).
    static CoopTask* self() noexcept { return static_cast<CoopTask*>(BasicCoopTask<StackAllocator>::self()); }

    /// Use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return or exceptions is to be preferred in most cases.
    /// @param code default exit code is default value of CoopTask<>'s template argument, use exit() to set a different value.
    static void exit(Result&& code = Result{}) noexcept { self()->_exit(std::move(code)); }

    /// Use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return or exceptions is to be preferred in most cases.
    /// @param code default exit code is default value of CoopTask<>'s template argument, use exit() to set a different value.
    static void exit(const Result& code) noexcept { self()->_exit(code); }
};

template<class StackAllocator> class CoopTask<void, StackAllocator> : public BasicCoopTask<StackAllocator>
{
public:
    using CoopTaskBase::taskfunction_t;

#if defined(ARDUINO)
    CoopTask(const String& name, CoopTaskBase::taskfunction_t func, size_t stackSize = BasicCoopTask<StackAllocator>::DEFAULTTASKSTACKSIZE) :
#else
    CoopTask(const std::string& name, CoopTaskBase::taskfunction_t func, size_t stackSize = BasicCoopTask<StackAllocator>::DEFAULTTASKSTACKSIZE) :
#endif
        BasicCoopTask<StackAllocator>(name, func, stackSize)
    {
    }

    /// @returns: a pointer to the CoopTask instance that is running. nullptr if not called from a CoopTask function (running() == false).
    static CoopTask* self() noexcept { return static_cast<CoopTask*>(BasicCoopTask<StackAllocator>::self()); }
};

/// A convenience function that creates a new CoopTask instance for the supplied task function, with the
/// given name and stack size, and schedules it.
/// @returns: the pointer to the new CoopTask instance, or nullptr if the creation or preparing for scheduling failed.
template<typename Result = int, class StackAllocator = CoopTaskStackAllocator>
CoopTask<Result, StackAllocator>* createCoopTask(
#if defined(ARDUINO)
    const String& name, typename CoopTask<Result, StackAllocator>::taskfunction_t func, size_t stackSize = CoopTaskBase::DEFAULTTASKSTACKSIZE)
#else
const std::string& name, typename CoopTask<Result, StackAllocator>::taskfunction_t func, size_t stackSize = CoopTaskBase::DEFAULTTASKSTACKSIZE)
#endif
{
    auto task = new CoopTask<Result, StackAllocator>(name, func, stackSize);
    if (task && task->scheduleTask()) return task;
    delete task;
    return nullptr;
}

#endif // __CoopTask_h
