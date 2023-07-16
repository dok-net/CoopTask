#pragma once
/*
task_completion_source.h - Implementation of a C++20 async coroutines task completion source.
Copyright (c) 2023 Dirk O. Kaar. All rights reserved.

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

#include <atomic>
#include <memory>

namespace ghostl
{
template<typename T = void>
struct task_completion_source
{
private:
    struct state_type
    {
        explicit state_type() {}
        state_type(const state_type&)                = delete;
        state_type(state_type&& other) noexcept      = delete;
        auto                                 operator=(const state_type&) -> state_type& = delete;
        auto                                 operator=(state_type&& other) noexcept -> state_type& = delete;
        std::atomic<bool>                    is_set{false};
        std::atomic<bool>                    ready{false};
        std::shared_ptr<T>                   value;
        std::atomic<std::coroutine_handle<>> coroutine{nullptr};
    };
    struct awaiter
    {
        explicit awaiter() {}
        explicit awaiter(std::shared_ptr<state_type>& _state) : state(_state) {}
        awaiter(const awaiter&)           = default;
        awaiter(awaiter&& other) noexcept = default;
        auto                        operator=(const awaiter&) -> awaiter& = default;
        auto                        operator=(awaiter&& other) noexcept -> awaiter& = default;
        std::shared_ptr<state_type> state;

        bool await_ready() const noexcept { return state->ready.exchange(true); }
        void await_suspend(std::coroutine_handle<> handle) const noexcept
        {
            state->coroutine.store(handle);
            state->ready.store(false);
        }
        T await_resume() const noexcept { return *state->value; }
    };

    std::shared_ptr<state_type> state{std::make_shared<state_type>()};

public:
    task_completion_source() {}
    task_completion_source(const task_completion_source&)           = default;
    task_completion_source(task_completion_source&& other) noexcept = default;
    auto operator=(const task_completion_source&) -> task_completion_source& = default;
    auto operator=(task_completion_source&& other) noexcept -> task_completion_source& = default;
    void set_value(const T& v) const
    {
        for (bool expect{false}; !state->is_set.compare_exchange_strong(expect, true);)
            return;
        state->value = std::make_shared<T>(v);
        std::atomic_thread_fence(std::memory_order_release);
        for (bool expect{false}; !state->ready.compare_exchange_weak(expect, true); expect = false) {}
    }
    void set_value(T&& v) const
    {
        for (bool expect{false}; !state->is_set.compare_exchange_strong(expect, true);)
            return;
        state->value = std::make_shared<T>(std::move(v));
        std::atomic_thread_fence(std::memory_order_release);
        for (bool expect{false}; !state->ready.compare_exchange_weak(expect, true); expect = false) {}
    }
    auto               handle() const { return state->coroutine.load(); }
    [[nodiscard]] auto token() { return awaiter(state); }
};

template<>
struct task_completion_source<void>
{
private:
    struct state_type
    {
        explicit state_type() {}
        state_type(const state_type&)                = delete;
        state_type(state_type&& other) noexcept      = delete;
        auto                                 operator=(const state_type&) -> state_type& = delete;
        auto                                 operator=(state_type&& other) noexcept -> state_type& = delete;
        std::atomic<bool>                    is_set{false};
        std::atomic<bool>                    ready{false};
        std::atomic<std::coroutine_handle<>> coroutine{nullptr};
    };
    struct awaiter
    {
        explicit awaiter(std::shared_ptr<state_type>& _state) : state(_state) {}
        awaiter(const awaiter&)           = default;
        awaiter(awaiter&& other) noexcept = default;
        auto                        operator=(const awaiter&) -> awaiter& = default;
        auto                        operator=(awaiter&& other) noexcept -> awaiter& = default;
        std::shared_ptr<state_type> state;

        bool await_ready() const noexcept { return state->ready.exchange(true); }
        void await_suspend(std::coroutine_handle<> handle) const noexcept
        {
            state->coroutine.store(handle);
            state->ready.store(false);
        }
        constexpr void await_resume() const noexcept {}
    };

    std::shared_ptr<state_type> state{std::make_shared<state_type>()};

public:
    task_completion_source() {}
    task_completion_source(const task_completion_source&)           = default;
    task_completion_source(task_completion_source&& other) noexcept = default;
    auto operator=(const task_completion_source&) -> task_completion_source& = default;
    auto operator=(task_completion_source&& other) noexcept -> task_completion_source& = default;
    void set_value() const
    {
        for (bool expect{false}; !state->is_set.compare_exchange_strong(expect, true);)
            return;
        std::atomic_thread_fence(std::memory_order_release);
        for (bool expect{false}; !state->ready.compare_exchange_weak(expect, true); expect = false) {}
    }
    auto               handle() const { return state->coroutine.load(); }
    [[nodiscard]] auto token() { return awaiter(state); }
};

} // namespace ghostl
