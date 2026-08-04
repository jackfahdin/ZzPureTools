// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#include "spdlog/sinks/async_sink.h"

#include <algorithm>
#include <cassert>
#include <future>
#include <memory>
#include <stdexcept>
#include <utility>

#include "spdlog/common.h"
#include "spdlog/details/mpmc_blocking_q.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/spdlog.h"

SPDLOG_NAMESPACE_BEGIN
namespace sinks {

async_sink::async_sink(config async_config)
    : config_(std::move(async_config)) {
    if (config_.queue_size == 0 || config_.queue_size > max_queue_size) {
        throw spdlog_ex("async_sink: invalid queue size");
    }
    if (config_.custom_err_handler) {
        err_helper_.set_err_handler(config_.custom_err_handler);
    }

    q_ = std::make_unique<queue_t>(config_.queue_size);
    worker_thread_ = std::thread([this] {
        if (config_.on_thread_start) config_.on_thread_start();
        this->backend_loop_();
        if (config_.on_thread_stop) config_.on_thread_stop();
    });
}

async_sink::~async_sink() {
    try {
        q_->enqueue_control(async_log_msg(async_log_msg::type::terminate));
        worker_thread_.join();
    } catch (...) {
        terminate_worker_ = true;  // as last resort, stop the worker thread using terminate_worker_ flag.
#ifndef NDEBUG
        printf("Exception in ~async_sink()\n");
#endif
    }
}

void async_sink::log(const details::log_msg &msg) { enqueue_message_(async_log_msg(async_log_msg::type::log, msg)); }

void async_sink::flush() {
    q_->enqueue_control(details::async_log_msg(async_log_msg::type::flush));
}

bool async_sink::flush_and_wait(const std::chrono::milliseconds timeout) {
    if (timeout < std::chrono::milliseconds::zero()) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    auto completion = std::make_shared<std::promise<void>>();
    auto completed = completion->get_future();
    try {
        if (!q_->enqueue_control_until(
                async_log_msg(async_log_msg::type::flush, completion),
                deadline)) {
            return false;
        }
        if (completed.wait_until(deadline) != std::future_status::ready) {
            return false;
        }
        completed.get();
        return true;
    } catch (...) {
        return false;
    }
}

void async_sink::set_pattern(const std::string &pattern) { set_formatter(std::make_unique<pattern_formatter>(pattern)); }

void async_sink::set_formatter(std::unique_ptr<formatter> formatter) {
    const auto &sinks = config_.sinks;
    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        if (std::next(it) == sinks.end()) {
            // last element - we can move it.
            (*it)->set_formatter(std::move(formatter));
            break;  // to prevent clang-tidy warning
        }
        (*it)->set_formatter(formatter->clone());
    }
}

bool async_sink::wait_all(const std::chrono::milliseconds timeout) const {
    using std::chrono::steady_clock;
    constexpr std::chrono::milliseconds sleep_duration(5);
    const auto start_time = steady_clock::now();
    while (q_->size() > 0) {
        auto elapsed = steady_clock::now() - start_time;
        if (elapsed > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::min(sleep_duration, timeout));
    }
    return true;
}

void async_sink::wait_all() const {
    while (!wait_all(std::chrono::milliseconds(10))) { /* empty */
    }
}

size_t async_sink::get_overrun_counter() const { return q_->overrun_counter(); }

void async_sink::reset_overrun_counter() const { q_->reset_overrun_counter(); }

size_t async_sink::get_discard_counter() const { return q_->discard_counter(); }

void async_sink::reset_discard_counter() const { q_->reset_discard_counter(); }

const async_sink::config &async_sink::get_config() const { return config_; }

async_sink::config &async_sink::get_config() { return config_; }

// private methods
void async_sink::enqueue_message_(details::async_log_msg &&msg) const {
    switch (config_.policy) {
        case overflow_policy::block:
            q_->enqueue(std::move(msg));
            break;
        case overflow_policy::overrun_oldest:
            q_->enqueue_nowait(std::move(msg), [](const async_log_msg &queued) {
                return queued.message_type() != async_log_msg::type::log;
            });
            break;
        case overflow_policy::discard_new:
            q_->enqueue_if_have_room(std::move(msg));
            break;
        default:
            assert(false);
            throw spdlog_ex("async_sink: invalid overflow policy");
    }
}

void async_sink::backend_loop_() {
    details::async_log_msg incoming_msg;
    while (!terminate_worker_) {
        q_->dequeue(incoming_msg);
        switch (incoming_msg.message_type()) {
            case async_log_msg::type::log:
                backend_log_(incoming_msg);
                break;
            case async_log_msg::type::flush:
                if (const auto &completion = incoming_msg.completion()) {
                    try {
                        if (backend_flush_()) {
                            completion->set_value();
                        } else {
                            completion->set_exception(std::make_exception_ptr(
                                std::runtime_error("async backend flush failed")));
                        }
                    } catch (...) {
                    }
                } else {
                    static_cast<void>(backend_flush_());
                }
                break;
            case async_log_msg::type::terminate:
                return;
            default:
                assert(false);
        }
    }
}

void async_sink::backend_log_(const details::log_msg &msg) {
    for (const auto &sink : config_.sinks) {
        if (sink->should_log(msg.log_level)) {
            try {
                sink->log(msg);
            } catch (const std::exception &ex) {
                err_helper_.handle_ex("async log", msg.source, ex);
            } catch (...) {
                err_helper_.handle_unknown_ex("async log", msg.source);
            }
        }
    }
}

bool async_sink::backend_flush_() {
    bool success = true;
    for (const auto &sink : config_.sinks) {
        try {
            sink->flush();
        } catch (const std::exception &ex) {
            success = false;
            try {
                err_helper_.handle_ex("async flush", source_loc{}, ex);
            } catch (...) {
            }
        } catch (...) {
            success = false;
            try {
                err_helper_.handle_unknown_ex("async flush", source_loc{});
            } catch (...) {
            }
        }
    }
    return success;
}
}  // namespace sinks
SPDLOG_NAMESPACE_END
