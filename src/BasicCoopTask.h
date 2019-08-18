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

class BasicCoopTask : public CoopTaskBase
{
protected:
#ifndef _MSC_VER
    bool allocateStack();
    void disposeStack() { delete[] taskStackTop; }
#endif

public:
#ifdef ARDUINO
    BasicCoopTask(const String& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#else
    BasicCoopTask(const std::string& name, taskfunction_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
#endif
        CoopTaskBase(name, _func, stackSize)
    {
#ifndef _MSC_VER
        allocateStack();
#endif
    }
    BasicCoopTask(const BasicCoopTask&) = delete;
    BasicCoopTask& operator=(const BasicCoopTask&) = delete;
    ~BasicCoopTask()
    {
#ifndef _MSC_VER
        disposeStack();
#endif
    }
};

#endif // __BasicCoopTask_h
