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
#if !defined(_MSC_VER) && !defined(ESP32)
    static char* allocateStack(uint32_t stackSize);
    static void disposeStack(char* stackTop) { delete[] stackTop; }
#endif
};

template<class StackAllocator = CoopTaskStackAllocator> class BasicCoopTask : public CoopTaskBase
{
protected:
    StackAllocator stackAllocator;
public:
#ifdef ARDUINO
    BasicCoopTask(const String& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    BasicCoopTask(const std::string& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        CoopTaskBase(name, _func, stackSize)
    {
#if !defined(_MSC_VER) && !defined(ESP32)
        taskStackTop = stackAllocator.allocateStack(stackSize);
#endif
    }
    BasicCoopTask(const BasicCoopTask&) = delete;
    BasicCoopTask& operator=(const BasicCoopTask&) = delete;
    ~BasicCoopTask()
    {
#if !defined(_MSC_VER) && !defined(ESP32)
        stackAllocator.disposeStack(taskStackTop);
#endif
    }
};

#endif // __BasicCoopTask_h
