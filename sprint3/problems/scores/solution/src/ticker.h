#pragma once

#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <chrono>
#include <memory>
#include <functional>

namespace app {

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(std::shared_ptr<Strand> strand, std::chrono::milliseconds period, Handler handler)
        : strand_(strand)
        , timer_(*strand, period)
        , period_(period)
        , handler_(handler) {}

    void Start() {
        last_tick_ = std::chrono::steady_clock::now();
        ScheduleTick();
    }

private:
    void ScheduleTick() {
        auto self = shared_from_this();
        timer_.expires_after(period_);
        timer_.async_wait(boost::asio::bind_executor(*strand_, [self](const boost::system::error_code& ec) {
            if (!ec) {
                auto current_time = std::chrono::steady_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - self->last_tick_);
                self->last_tick_ = current_time;

                self->handler_(delta);
                self->ScheduleTick();
            }
        }));
    }

    std::shared_ptr<Strand> strand_;
    boost::asio::steady_timer timer_;
    std::chrono::milliseconds period_;
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

} // namespace app
