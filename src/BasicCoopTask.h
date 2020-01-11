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
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
public:
    static char* allocateStack(unsigned stackSize);
    static void disposeStack(char* stackTop) { delete[] stackTop; }
#endif
};

template<char StackTop[], unsigned StackSize> class CoopTaskStackAllocatorFromBSS
{
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
public:
    static char* allocateStack(unsigned stackSize)
    {
        return (StackSize == (stackSize + (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE))) ?
            StackTop : nullptr;
    }
    static void disposeStack(char* stackTop) { }
#endif
};

class CoopTaskStackAllocatorFromLoopBase
{
#if (defined(ARDUINO) && !defined(ESP32_FREERTOS)) || defined(__GNUC__)
protected:
    static char* allocateStack(unsigned loopReserve, unsigned stackSize);
#endif
public:
    static void disposeStack(char* stackTop) { }
};

template<unsigned LoopReserve = (CoopTaskBase::DEFAULTTASKSTACKSIZE / 2)>
class CoopTaskStackAllocatorFromLoop : public CoopTaskStackAllocatorFromLoopBase
{
#if (defined(ARDUINO) && !defined(ESP32_FREERTOS)) || defined(__GNUC__)
public:
    static char* allocateStack(unsigned stackSize)
    {
        return CoopTaskStackAllocatorFromLoopBase::allocateStack(LoopReserve, stackSize);
    }
#endif
};

template<class StackAllocator = CoopTaskStackAllocator> class BasicCoopTask : public CoopTaskBase
{
protected:
    StackAllocator stackAllocator;
public:
#ifdef ARDUINO
    BasicCoopTask(const String& name, taskfunction_t _func, unsigned stackSize = DEFAULTTASKSTACKSIZE) :
#else
    BasicCoopTask(const std::string& name, taskfunction_t _func, unsigned stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        CoopTaskBase(name, _func, stackSize)
    {
#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)
        taskStackTop = stackAllocator.allocateStack(stackSize);
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
};

#endif // __BasicCoopTask_h
