/*
BasicCoopTask.cpp - Implementation of cooperative scheduling tasks
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

#include "BasicCoopTask.h"
#ifdef ARDUINO
#include <alloca.h>
#endif

#ifndef _MSC_VER

bool BasicCoopTask::allocateStack()
{
    if (!cont || init) return false;
    if (taskStackTop) return true;
    if (taskStackSize <= MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE))
    {
#if defined(ESP8266) || defined(ESP32)
        taskStackTop = new (std::nothrow) char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
#else
        taskStackTop = new char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
#endif
    }
    return taskStackTop;
}

#endif // _MSC_VER
