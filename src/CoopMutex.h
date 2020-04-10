/*
CoopMutex.h - Implementation of a mutex and an RAII lock for cooperative scheduling tasks
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

#ifndef __CoopMutex_h
#define __CoopMutex_h

#include "CoopSemaphore.h"

/// A mutex that is safe to use from CoopTasks.
class CoopMutex : private CoopSemaphore
{
protected:
    std::atomic<CoopTaskBase*> owner;

public:
    CoopMutex(size_t maxPending = 10) : CoopSemaphore(1, maxPending), owner(nullptr) {}
    CoopMutex(const CoopMutex&) = delete;
    CoopMutex& operator=(const CoopMutex&) = delete;

    /// @returns: true, or false, if the current task does not own the mutex.
    bool unlock()
    {
        if (CoopTaskBase::running() && CoopTaskBase::self() == owner.load() && post())
        {
            owner.store(nullptr);
            return true;
        }
        return false;
    }

    /// @returns: true if the mutex becomes locked. false if it is already locked by the same task, or the maximum number of pending tasks is exceeded.
    bool lock()
    {
        if (CoopTaskBase::running() && CoopTaskBase::self() != owner.load() && wait())
        {
            owner.store(CoopTaskBase::self());
            return true;
        }
        return false;
    }

    /// @returns: true if the mutex becomes freshly locked without waiting, otherwise false.
    bool try_lock()
    {
        if (CoopTaskBase::running() && CoopTaskBase::self() != owner.load() && try_wait())
        {
            owner.store(CoopTaskBase::self());
            return true;
        }
        return false;
    }
};

/// A RAII CoopMutex lock class.
class CoopMutexLock {
protected:
    CoopMutex& mutex;
    bool locked;
public:
    /// The constructor returns if the mutex was locked, or locking failed.
    explicit CoopMutexLock(CoopMutex& _mutex) : mutex(_mutex) {
        locked = mutex.lock();
    }
    CoopMutexLock() = delete;
    CoopMutexLock(const CoopMutexLock&) = delete;
    CoopMutexLock& operator=(const CoopMutexLock&) = delete;
    /// @returns: true if the mutex became locked, potentially after blocking, otherwise false.
    operator bool() const {
        return locked;
    }
    /// The destructor unlocks the mutex.
    ~CoopMutexLock() {
        if (locked) mutex.unlock();
    }
};

#endif // __CoopMutex_h
