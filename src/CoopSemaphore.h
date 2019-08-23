/*
CoopSemaphore.h - Implementation of a semaphore for cooperative scheduling tasks
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

#ifndef __CoopSemaphore_h
#define __CoopSemaphore_h

#include "CoopTaskBase.h"
#if defined(ESP8266)
#include "circular_queue/circular_queue.h"
#include <interrupts.h>
using esp8266::InterruptLock;
#elif defined(ESP32) || !defined(ARDUINO)
#include "circular_queue/circular_queue.h"
using std::min;
#else
class InterruptLock {
public:
    InterruptLock() {
        noInterrupts();
    }
    ~InterruptLock() {
        interrupts();
    }
};
namespace std
{
    extern "C" void atomic_thread_fence(std::memory_order) noexcept {}
    template< typename T > using function = T *;
}

template< typename T >
class circular_queue
{
public:
    /*!
        @brief  Constructs a queue of the given maximum capacity.
    */
    circular_queue(const size_t capacity) : m_bufSize(capacity + 1), m_buffer(new T[m_bufSize])
    {
        m_inPos.store(0);
        m_outPos.store(0);
    }
    ~circular_queue()
    {
        m_buffer.reset();
    }
    circular_queue(const circular_queue&) = delete;
    circular_queue& operator=(const circular_queue&) = delete;

    /*!
        @brief	Get a snapshot number of elements that can be retrieved by pop.
    */
    size_t available() const
    {
        int avail = static_cast<int>(m_inPos.load() - m_outPos.load());
        if (avail < 0) avail += m_bufSize;
        return avail;
    }

    /*!
        @brief	Move the rvalue parameter into the queue.
        @return true if the queue accepted the value, false if the queue
                was full.
    */
    bool IRAM_ATTR push(T&& val);

    /*!
        @brief	Push a copy of the parameter into the queue.
        @return true if the queue accepted the value, false if the queue
                was full.
    */
    bool IRAM_ATTR push(const T& val)
    {
        return push(T(val));
    }

    /*!
        @brief	Pop the next available element from the queue.
        @return An rvalue copy of the popped element, or a default
                value of type T if the queue is empty.
    */
    T pop();

    /*!
        @brief	Iterate over and remove each available element from queue,
                calling back fun with an rvalue reference of every single element.
    */
    void for_each(std::function<void(T&&)> fun);

    /*!
        @brief	In reverse order, iterate over, pop and optionally requeue each available element from the queue,
                calling back fun with a reference of every single element.
                Requeuing is dependent on the return boolean of the callback function. If it
                returns true, the requeue occurs.
    */
    bool for_each_rev_requeue(std::function<bool(T&)> fun);

protected:
    const T defaultValue = {};
    unsigned m_bufSize;
    std::unique_ptr<T> m_buffer;
    std::atomic<unsigned> m_inPos;
    std::atomic<unsigned> m_outPos;
};

template< typename T >
bool IRAM_ATTR circular_queue<T>::push(T&& val)
{
    const auto inPos = m_inPos.load(std::memory_order_acquire);
    const unsigned next = (inPos + 1) % m_bufSize;
    if (next == m_outPos.load(std::memory_order_relaxed)) {
        return false;
    }

    std::atomic_thread_fence(std::memory_order_acquire);

    m_buffer[inPos] = std::move(val);

    std::atomic_thread_fence(std::memory_order_release);

    m_inPos.store(next, std::memory_order_release);
    return true;
}

template< typename T >
T circular_queue<T>::pop()
{
    const auto outPos = m_outPos.load(std::memory_order_acquire);
    if (m_inPos.load(std::memory_order_relaxed) == outPos) return defaultValue;

    std::atomic_thread_fence(std::memory_order_acquire);

    auto val = std::move(m_buffer[outPos]);

    std::atomic_thread_fence(std::memory_order_release);

    m_outPos.store((outPos + 1) % m_bufSize, std::memory_order_release);
    return val;
}

template< typename T >
void circular_queue<T>::for_each(std::function<void(T&&)> fun)
{
    auto outPos = m_outPos.load(std::memory_order_acquire);
    const auto inPos = m_inPos.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    while (outPos != inPos)
    {
        fun(std::move(m_buffer[outPos]));
        std::atomic_thread_fence(std::memory_order_release);
        outPos = (outPos + 1) % m_bufSize;
        m_outPos.store(outPos, std::memory_order_release);
    }
}

template< typename T >
bool circular_queue<T>::for_each_rev_requeue(std::function<bool(T&)> fun)
{
    auto inPos0 = circular_queue<T>::m_inPos.load(std::memory_order_acquire);
    auto outPos = circular_queue<T>::m_outPos.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (outPos == inPos0) return false;
    auto pos = inPos0;
    auto outPos1 = inPos0;
    const auto posDecr = circular_queue<T>::m_bufSize - 1;
    do {
        pos = (pos + posDecr) % circular_queue<T>::m_bufSize;
        T&& val = std::move(circular_queue<T>::m_buffer[pos]);
        if (fun(val))
        {
            outPos1 = (outPos1 + posDecr) % circular_queue<T>::m_bufSize;
            if (outPos1 != pos) circular_queue<T>::m_buffer[outPos1] = std::move(val);
        }
    } while (pos != outPos);
    circular_queue<T>::m_outPos.store(outPos1, std::memory_order_release);
    return true;
}

#endif

#if !defined(ESP32) && !defined(ESP8266)
#define ICACHE_RAM_ATTR
#define IRAM_ATTR
#endif

/// A semaphore that is safe to use from CoopTasks.
/// Only post() is safe to use from interrupt service routines,
/// or concurrent OS threads that must synchronized with the singled thread running CoopTasks.
class CoopSemaphore
{
protected:
    std::atomic<unsigned> value;
    std::atomic<CoopTaskBase*> pendingTask0;
    std::unique_ptr<circular_queue<CoopTaskBase*>> pendingTasks;
public:
    /// @param val the initial value of the semaphore.
    /// @param maxPending the maximum supported number of concurrently waiting tasks.
    CoopSemaphore(unsigned val, unsigned maxPending = 10) : value(val), pendingTask0(nullptr), pendingTasks(new circular_queue<CoopTaskBase*>(maxPending)) {}
    CoopSemaphore(const CoopSemaphore&) = delete;
    CoopSemaphore& operator=(const CoopSemaphore&) = delete;
    ~CoopSemaphore()
    {
        // wake up all queued tasks
        pendingTasks->for_each([](CoopTaskBase*&& task) { task->sleep(false); });
        pendingTasks.reset();
    }

    /// post() is the only operation that is allowed from an interrupt service routine,
    /// or a concurrent OS thread that is synchronized with the singled thread running CoopTasks.
    bool IRAM_ATTR post()
    {
        CoopTaskBase* pendingTask;
#if !defined(ESP32) && defined(ARDUINO)
        {
            InterruptLock lock;
            unsigned val = value.load();
            value.store(val + 1);
            pendingTask = pendingTask0.load();
            pendingTask0.store(nullptr);
        }
#else
        unsigned val = 0;
        while (!value.compare_exchange_weak(val, val + 1)) {}
        pendingTask = pendingTask0.exchange(nullptr);
#endif
        if (pendingTask && pendingTask->sleeping()) scheduleTask(pendingTask, true);
        return true;
    }

    /// @returns: true if sucessfully acquired the semaphore, either immediately or after sleeping. false if maximum number of pending tasks is exceeded.
    bool wait()
    {
        for (;;)
        {
            auto& self = CoopTaskBase::self();
            unsigned val;
#if !defined(ESP32) && defined(ARDUINO)
            {
                InterruptLock lock;
                val = value.load();
                if (val)
                {
                    value.store(val - 1);
                }
            }
#else
            val = 1;
            while (val && !value.compare_exchange_weak(val, val - 1)) {}
#endif
            if (!val)
            {
                if (!pendingTasks->push(&self))
                {
                    return false;
                }
                self.sleep(true);
            }
            CoopTaskBase* pendingTask = nullptr;
            const bool forcePop = val > 1;
            for (;;)
            {
                if (pendingTasks->available())
                {
#if !defined(ESP32) && defined(ARDUINO)
                    InterruptLock lock;
                    if (!(pendingTask = pendingTask0.load()) || forcePop)
                    {
                        pendingTask0.store(pendingTasks->pop());
                    }
#else
                    bool exchd = false;
                    while ((!pendingTask || forcePop) && !(exchd = pendingTask0.compare_exchange_strong(pendingTask, pendingTasks->peek()))) {}
                    if (exchd) pendingTasks->pop();
#endif
                }
                else
                {
#if !defined(ESP32) && defined(ARDUINO)
                    InterruptLock lock;
                    pendingTask = pendingTask0.load();
                    pendingTask0.store(nullptr);
#else
                    while (!pendingTask0.compare_exchange_weak(pendingTask, nullptr)) {}
#endif
                    if (val) val = 1;
                }
                if (val <= 1) break;
                if (!pendingTask)
                {
                    continue;
                }
                val -= 1;

                if (&self == pendingTask)
                {
                    self.sleep(false);
                }
                else if (pendingTask->sleeping())
                {
                    scheduleTask(pendingTask, true);
                }
            }
            if (val) return true;
#ifdef ESP32
            yield();
#else
            CoopTaskBase::yield();
#endif
        }
    }

    /// @returns: true if the semaphore was acquired immediately, otherwise false.
    bool try_wait()
    {
        unsigned val;
#if !defined(ESP32) && defined(ARDUINO)
        {
            InterruptLock lock;
            val = value.load();
            if (val)
            {
                value.store(val - 1);
            }
        }
#else
        val = 1;
        while (val && !value.compare_exchange_weak(val, val - 1)) {}
#endif
        return val;
    }
};

#endif // __CoopSemaphore_h
