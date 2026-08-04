// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <utility>

#include "./circular_q.h"

SPDLOG_NAMESPACE_BEGIN
namespace details {

template <typename T>
class mpmc_blocking_queue {
public:
    using item_type = T;

    explicit mpmc_blocking_queue(size_t max_items)
        : q_(max_items) {}

    // Block-policy data waits behind control messages already waiting for room.
    void enqueue(T &&item) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        pop_cv_.wait(lock, [this] {
            return !q_.full() && control_waiters_ == 0;
        });
        q_.push_back(std::move(item));
        push_cv_.notify_one();
    }

    // Overrun-policy data may only replace another data message.
    template <typename IsControl>
    void enqueue_nowait(T &&item, IsControl is_control) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (control_waiters_ > 0
            || (q_.full() && is_control(q_.front()))) {
            discard_counter_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        q_.push_back(std::move(item));
        push_cv_.notify_one();
    }

    // Discard-policy data never consumes space reserved by a control waiter.
    void enqueue_if_have_room(T &&item) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (q_.full() || control_waiters_ > 0) {
            discard_counter_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        q_.push_back(std::move(item));
        push_cv_.notify_one();
    }

    // Control messages are never subject to a data overflow policy.
    void enqueue_control(T &&item) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        ++control_waiters_;
        try {
            pop_cv_.wait(lock, [this] { return !q_.full(); });
            q_.push_back(std::move(item));
            --control_waiters_;
        } catch (...) {
            --control_waiters_;
            pop_cv_.notify_all();
            throw;
        }
        push_cv_.notify_one();
        pop_cv_.notify_all();
    }

    template <typename Clock, typename Duration>
    [[nodiscard]] bool enqueue_control_until(
        T &&item,
        const std::chrono::time_point<Clock, Duration> &deadline) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        ++control_waiters_;
        try {
            if (!pop_cv_.wait_until(
                    lock, deadline, [this] { return !q_.full(); })) {
                --control_waiters_;
                pop_cv_.notify_all();
                return false;
            }
            q_.push_back(std::move(item));
            --control_waiters_;
        } catch (...) {
            --control_waiters_;
            pop_cv_.notify_all();
            throw;
        }
        push_cv_.notify_one();
        pop_cv_.notify_all();
        return true;
    }

    // Return true if an item was dequeued before the timeout.
    bool dequeue_for(T &popped_item, std::chrono::milliseconds wait_duration) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!push_cv_.wait_for(
                lock, wait_duration, [this] { return !q_.empty(); })) {
            return false;
        }
        popped_item = std::move(q_.front());
        q_.pop_front();
        pop_cv_.notify_all();
        return true;
    }

    void dequeue(T &popped_item) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        push_cv_.wait(lock, [this] { return !q_.empty(); });
        popped_item = std::move(q_.front());
        q_.pop_front();
        pop_cv_.notify_all();
    }

    size_t overrun_counter() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return q_.overrun_counter();
    }

    size_t discard_counter() const {
        return discard_counter_.load(std::memory_order_relaxed);
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return q_.size();
    }

    void reset_overrun_counter() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        q_.reset_overrun_counter();
    }

    void reset_discard_counter() {
        discard_counter_.store(0, std::memory_order_relaxed);
    }

private:
    std::mutex queue_mutex_;
    std::condition_variable push_cv_;
    std::condition_variable pop_cv_;
    circular_q<T> q_;
    std::atomic<size_t> discard_counter_{0};
    size_t control_waiters_{0};
};

} // namespace details
SPDLOG_NAMESPACE_END
