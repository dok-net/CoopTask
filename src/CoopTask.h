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

template<typename Result = int> class CoopTask : public BasicCoopTask
{
public:
#if defined(ESP8266) || defined(ESP32) || !defined(ARDUINO)
    using taskfunction_t = std::function< Result() >;
#else
    using taskfunction_t = Result(*)();
#endif

#if defined(ARDUINO)
    CoopTask(const String& name, CoopTask::taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    CoopTask(const std::string& name, CoopTask::taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        // Wrap _func into _exit() to capture return value as exit code
        BasicCoopTask(name, captureFuncReturn, stackSize), func(_func)
    {
    }

protected:
    Result _exitCode;

    static void captureFuncReturn() noexcept
    {
#if defined(ESP32) || !defined(ARDUINO)
        try {
#endif
            self()._exitCode = self().func();
#if defined(ESP32) || !defined(ARDUINO)
        }
        catch (Result code)
        {
            self()._exitCode = code;
        }
#endif
    }
    void _exit(Result&& code = Result()) noexcept
    {
        _exitCode = std::move(code);
        BasicCoopTask::_exit();
    }
    void _exit(const Result& code) noexcept
    {
        _exitCode = code;
        BasicCoopTask::_exit();
    }

private:
    taskfunction_t func;

public:
    // @returns: The exit code is either the return value of of the task function, or set by using the exit() function.
    Result exitCode() const noexcept { return _exitCode; }

    // @returns: a reference to CoopTask instance that is running. Undefined if not called from a CoopTask function (running() == false).
    static CoopTask& self() noexcept { return static_cast<CoopTask&>(BasicCoopTask::self()); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return or exceptions is to be preferred in most cases.
    // @param code default exit code is default value of CoopTask<>'s template argument, use exit() to set a different value.
    static void exit(Result&& code = Result()) noexcept { self()._exit(std::move(code)); }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return or exceptions is to be preferred in most cases.
    // @param code default exit code is default value of CoopTask<>'s template argument, use exit() to set a different value.
    static void exit(const Result& code) noexcept { self()._exit(code); }
};

/// A convenience function that creates a matching CoopTask for the supplied task function, with the
/// given name and stack size, and adds it to the scheduler.
// @returns: the pointer to the new CoopTask instance, or null if the creation or scheduling failed.
template<typename Result = int> CoopTask<Result> * scheduleTask(
#if defined(ARDUINO)
    const String & name, typename CoopTask<Result>::taskfunction_t func, uint32_t stackSize = BasicCoopTask::DEFAULTTASKSTACKSIZE)
#else
const std::string & name, typename CoopTask<Result>::taskfunction_t func, uint32_t stackSize = BasicCoopTask::DEFAULTTASKSTACKSIZE)
#endif
{
    auto task = new CoopTask<Result>(name, func, stackSize);
    if (task && scheduleTask(task)) return task;
    delete task;
    return nullptr;
}

#endif // __CoopTask_h
