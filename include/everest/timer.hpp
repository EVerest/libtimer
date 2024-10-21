// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2021 Pionix GmbH and Contributors to EVerest
#ifndef EVEREST_TIMER_HPP
#define EVEREST_TIMER_HPP

#include <boost/asio.hpp>
#include <chrono>
#include <date/date.h>
#include <date/tz.h>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace Everest {

template <typename Type, bool Exists> struct OptionalTimerMember {
    Type data;
};

template <typename Type> struct OptionalTimerMember<Type, false> {};

template <template <typename> typename Guard, typename Mutex, bool Enabled> struct OptionalGuard {
    OptionalGuard(OptionalTimerMember<Mutex, Enabled>& in_mutex) {
        if constexpr (Enabled) {
            guard.data = std::move(Guard<Mutex>(in_mutex.data));
        }
    }

    OptionalTimerMember<Guard<Mutex>, Enabled> guard;
};

class TimerExecutionContext {
public:
    boost::asio::io_context io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;
    std::unique_ptr<std::thread> timer_thread = nullptr;
    std::thread::id timer_thread_id;

public:
    TimerExecutionContext() : work(boost::asio::make_work_guard(this->io_context)) {
        this->timer_thread = std::make_unique<std::thread>([this]() {
            timer_thread_id = std::this_thread::get_id();
            this->io_context.run();
        });
    }

    ~TimerExecutionContext() noexcept(false) {
        if (std::this_thread::get_id() == timer_thread_id) {
            throw std::runtime_error("Trying to destruct TimerExecContext from the same thread it was created!");
        }

        this->io_context.stop();
        this->timer_thread->join();
    }

    boost::asio::io_context& get_io_context() {
        return this->io_context;
    }

public:
    static inline std::shared_ptr<TimerExecutionContext> get_unique_context() {
        return std::make_shared<TimerExecutionContext>();
    }

    static std::shared_ptr<TimerExecutionContext> get_shared_context() {
        static std::shared_ptr<TimerExecutionContext> context;
        static std::once_flag context_flags;

        std::call_once(context_flags, []() { context = std::make_shared<TimerExecutionContext>(); });

        return context;
    }
};

// template <typename TimerClock = date::steady_clock> class Timer {
template <typename TimerClock = date::utc_clock, bool ThreadSafe = false, bool SharedContext = false> class Timer {
private:
    std::unique_ptr<boost::asio::basic_waitable_timer<TimerClock>> timer = nullptr;
    std::function<void()> callback;
    std::function<void(const boost::system::error_code& e)> callback_wrapper;
    std::chrono::nanoseconds interval_nanoseconds;
    bool running = false;

    std::shared_ptr<TimerExecutionContext> context;

    OptionalTimerMember<std::mutex, ThreadSafe> mutex;

public:
    /// This timer will initialize a boost::asio::io_context
    explicit Timer() {
        if constexpr (SharedContext) {
            context = TimerExecutionContext::get_shared_context();
        } else {
            context = TimerExecutionContext::get_unique_context();
        }

        this->timer = std::make_unique<boost::asio::basic_waitable_timer<TimerClock>>(context->get_io_context());
    }

    explicit Timer(const std::function<void()>& callback) {
        if constexpr (SharedContext) {
            context = TimerExecutionContext::get_shared_context();
        } else {
            context = TimerExecutionContext::get_unique_context();
        }

        this->callback = callback;
        this->timer = std::make_unique<boost::asio::basic_waitable_timer<TimerClock>>(context->get_io_context());
    }

    explicit Timer(boost::asio::io_context* io_context) {
        this->timer = std::make_unique<boost::asio::basic_waitable_timer<TimerClock>>(*io_context);
    }

    explicit Timer(boost::asio::io_context* io_context, const std::function<void()>& callback) {
        this->callback = callback;
        this->timer = std::make_unique<boost::asio::basic_waitable_timer<TimerClock>>(*io_context);
    }

    ~Timer() {
        if (this->timer) {
            // stop asio timer
            this->timer->cancel();
        }
    }

    /// Executes the given callback at the given timepoint
    template <class Clock, class Duration = typename Clock::duration>
    void at(const std::function<void()>& callback, const std::chrono::time_point<Clock, Duration>& time_point) {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        this->stop_internal();
        this->callback = callback;

        this->at_internal(time_point);
    }

    /// Executes the at the given timepoint
    template <class Clock, class Duration = typename Clock::duration>
    void at(const std::chrono::time_point<Clock, Duration>& time_point) {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        at_internal<Clock, Duration>(time_point);
    }

    /// Execute the given callback peridically from now in the given interval
    template <class Rep, class Period>
    void interval(const std::function<void()>& callback, const std::chrono::duration<Rep, Period>& interval) {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        this->stop_internal();
        this->callback = callback;

        this->interval_internal(interval);
    }

    /// Execute peridically from now in the given interval
    template <class Rep, class Period> void interval(const std::chrono::duration<Rep, Period>& interval) {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        this->interval_internal(interval);
    }

    // Execute the given callback once after the given interval
    template <class Rep, class Period>
    void timeout(const std::function<void()>& callback, const std::chrono::duration<Rep, Period>& interval) {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        this->stop_internal();
        this->callback = callback;

        this->timeout_internal(interval);
    }

    // Execute the given callback once after the given interval
    template <class Rep, class Period> void timeout(const std::chrono::duration<Rep, Period>& interval) {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        this->timeout_internal(interval);
    }

    /// Stop timer from excuting its callback
    void stop() {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        stop_internal();
    }

    bool is_running() {
        OptionalGuard<std::unique_lock, std::mutex, ThreadSafe> optional_guard(this->mutex);

        return running;
    }

private:
    /// Executes the at the given timepoint
    template <class Clock, class Duration = typename Clock::duration>
    void at_internal(const std::chrono::time_point<Clock, Duration>& time_point) {
        this->stop_internal();

        if (this->callback == nullptr) {
            return;
        }

        if (this->timer) {
            running = true;

            // use asio timer
            this->timer->expires_at(time_point);
            this->timer->async_wait([this](const boost::system::error_code& e) {
                if (e) {
                    return;
                }

                this->callback();
                running = false;
            });
        }
    }

    template <class Rep, class Period> void interval_internal(const std::chrono::duration<Rep, Period>& interval) {
        this->stop_internal();

        this->interval_nanoseconds = interval;
        if (interval_nanoseconds == std::chrono::nanoseconds(0)) {
            return;
        }

        if (this->callback == nullptr) {
            return;
        }

        if (this->timer) {
            running = true;

            // use asio timer
            this->callback_wrapper = [this](const boost::system::error_code& e) {
                if (e) {
                    running = false;
                    return;
                }

                this->timer->expires_from_now(this->interval_nanoseconds);
                this->timer->async_wait(this->callback_wrapper);

                this->callback();
            };

            this->timer->expires_from_now(this->interval_nanoseconds);
            this->timer->async_wait(this->callback_wrapper);
        }
    }

    template <class Rep, class Period> void timeout_internal(const std::chrono::duration<Rep, Period>& interval) {
        this->stop_internal();

        if (this->callback == nullptr) {
            return;
        }

        if (this->timer) {
            running = true;

            // use asio timer
            this->timer->expires_from_now(interval);
            this->timer->async_wait([this](const boost::system::error_code& e) {
                if (e) {
                    running = false;
                    return;
                }

                this->callback();
                running = false;
            });
        }
    }

    void stop_internal() {
        if (this->timer) {
            // asio based timer
            this->timer->cancel();
        }

        running = false;
    }
};

using SteadyTimer = Timer<date::utc_clock>;
using SystemTimer = Timer<date::utc_clock>;
} // namespace Everest

#endif // EVEREST_TIMER_HPP
