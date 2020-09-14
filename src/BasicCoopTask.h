/*
BasicCoopTask.h - Implementation of cooperative scheduling tasks
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

#ifndef __BasicCoopTask_h
#define __BasicCoopTask_h

#include "CoopTaskBase.h"

class CoopTaskStackAllocator
{
public:
    static constexpr size_t DEFAULTTASKSTACKSIZE = CoopTaskBase::DEFAULTTASKSTACKSIZE;
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
    static char* allocateStack(size_t stackSize);
    static void disposeStack(char* stackTop) { delete[] stackTop; }
#endif
};

template<size_t StackSize = CoopTaskBase::DEFAULTTASKSTACKSIZE>
class CoopTaskStackAllocatorAsMember
{
public:
    static constexpr size_t DEFAULTTASKSTACKSIZE =
        (sizeof(unsigned) >= 4) ? ((StackSize + sizeof(unsigned) - 1) / sizeof(unsigned)) * sizeof(unsigned) : StackSize;

#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
protected:
    char _stackTop[DEFAULTTASKSTACKSIZE + (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE)];
public:
    char* allocateStack(size_t stackSize)
    {
        return (DEFAULTTASKSTACKSIZE >= stackSize) ?
            _stackTop : nullptr;
    }
    static void disposeStack(char* stackTop) { }
#endif
};

class CoopTaskStackAllocatorFromLoopBase
{
public:
    static constexpr size_t DEFAULTTASKSTACKSIZE = CoopTaskBase::DEFAULTTASKSTACKSIZE;
#if (defined(ARDUINO) && !defined(ESP32_FREERTOS)) || defined(__GNUC__)
protected:
    static char* allocateStack(size_t loopReserve, size_t stackSize);
#endif
public:
    static void disposeStack(char* stackTop) { }
};

template<size_t LoopReserve = (CoopTaskBase::DEFAULTTASKSTACKSIZE / 2)>
class CoopTaskStackAllocatorFromLoop : public CoopTaskStackAllocatorFromLoopBase
{
public:
    static constexpr size_t DEFAULTTASKSTACKSIZE = CoopTaskBase::DEFAULTTASKSTACKSIZE;
#if (defined(ARDUINO) && !defined(ESP32_FREERTOS)) || defined(__GNUC__)
    static char* allocateStack(size_t stackSize)
    {
        return CoopTaskStackAllocatorFromLoopBase::allocateStack(LoopReserve, stackSize);
    }
#endif
};

template<class StackAllocator = CoopTaskStackAllocator> class BasicCoopTask : public CoopTaskBase
{
public:
#ifdef ARDUINO
    BasicCoopTask(const String& name, taskfunction_t _func, size_t stackSize = StackAllocator::DEFAULTTASKSTACKSIZE) :
#else
    BasicCoopTask(const std::string& name, taskfunction_t _func, size_t stackSize = StackAllocator::DEFAULTTASKSTACKSIZE) :
#endif
        CoopTaskBase(name, _func, stackSize)
    {
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
        taskStackTop = stackAllocator.allocateStack(taskStackSize);
#endif
    }
    BasicCoopTask(const BasicCoopTask&) = delete;
    BasicCoopTask& operator=(const BasicCoopTask&) = delete;
    ~BasicCoopTask()
    {
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
        stackAllocator.disposeStack(taskStackTop);
#endif
    }
    /// Every task is entered into this list by scheduleTask(). It is removed when it exits
    /// or gets deleted.
    static const std::array< std::atomic<BasicCoopTask* >, MAXNUMBERCOOPTASKS>& getRunnableTasks()
    {
        // this is safe to do because CoopTaskBase ctor is protected.
        return reinterpret_cast<const std::array< std::atomic<BasicCoopTask* >, MAXNUMBERCOOPTASKS>&>(CoopTaskBase::getRunnableTasks());
    }
protected:
    StackAllocator stackAllocator;
};

#endif // __BasicCoopTask_h
