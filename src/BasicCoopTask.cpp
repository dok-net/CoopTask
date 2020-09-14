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

#if defined(ARDUINO) && !defined(ESP32_FREERTOS)
#include <alloca.h>
#endif

#if !defined(_MSC_VER) && !defined(ESP32_FREERTOS)

char* CoopTaskStackAllocator::allocateStack(size_t stackSize)
{
    char* stackTop = nullptr;
    if (stackSize <= CoopTaskBase::MAXSTACKSPACE - (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE))
    {
#if defined(ESP8266)
        stackTop = new (std::nothrow) char[stackSize + (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE)];
#else
        stackTop = new char[stackSize + (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE)];
#endif
    }
    return stackTop;
}

#endif // !defined(_MSC_VER) && !defined(ESP32_FREERTOS)

#if (defined(ARDUINO) && !defined(ESP32_FREERTOS)) || defined(__GNUC__)

char* CoopTaskStackAllocatorFromLoopBase::allocateStack(size_t loopReserve, size_t stackSize)
{
    char* bp = static_cast<char*>(alloca(
        (sizeof(unsigned) >= 4) ? ((loopReserve + sizeof(unsigned) - 1) / sizeof(unsigned)) * sizeof(unsigned) : loopReserve
    ));
    std::atomic_thread_fence(std::memory_order_release);
    char* stackTop = nullptr;
    if (stackSize <= CoopTaskBase::MAXSTACKSPACE - (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE))
    {
        stackTop = reinterpret_cast<char*>(
            reinterpret_cast<long unsigned>(bp) - stackSize + (CoopTaskBase::FULLFEATURES ? 2 : 1) * sizeof(CoopTaskBase::STACKCOOKIE));
    }
    return stackTop;
}

#endif // (defined(ARDUINO) && !defined(ESP32_FREERTOS)) || defined(__GNUC__)
