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

#ifndef _MSC_VER

char* CoopTaskStackAllocator::allocateStack(uint32_t stackSize)
{
    char* stackTop = nullptr;
    if (stackSize <= CoopTaskBase::MAXSTACKSPACE - 2 * sizeof(CoopTaskBase::STACKCOOKIE))
    {
#if defined(ESP8266) || defined(ESP32)
        stackTop = new (std::nothrow) char[stackSize + 2 * sizeof(CoopTaskBase::STACKCOOKIE)];
#else
        stackTop = new char[stackSize + 2 * sizeof(CoopTaskBase::STACKCOOKIE)];
#endif
    }
    return stackTop;
}

#endif // _MSC_VER
