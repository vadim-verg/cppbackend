#pragma once
#include <atomic>
#include <memory>
#include <functional>
#include <boost/asio.hpp>

#include "gascooker.h"
#include "ingredients.h"
#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;
using namespace std::chrono_literals;

class OrderContext : public std::enable_shared_from_this<OrderContext> {
public:
    using HotDogHandler = std::function<void(Result<HotDog>)>;

    OrderContext(net::io_context& io, Store& store, std::shared_ptr<GasCooker> cooker, int order_id, HotDogHandler handler)
        : io_(io)
        , store_(store)
        , cooker_(std::move(cooker))
        , order_id_(order_id)
        , handler_(std::move(handler))
        , strand_(net::make_strand(io)) {}

    void Start() {
        try {
            sausage_ = store_.GetSausage();
            bread_ = store_.GetBread();

            // Передаем оригинальную плиту, у которой мы принудительно оживили shared_from_this()
            sausage_->StartFry(*cooker_, net::bind_executor(strand_, [self = shared_from_this()] {
                self->OnSausageFryingStarted();
            }));

            bread_->StartBake(*cooker_, net::bind_executor(strand_, [self = shared_from_this()] {
                self->OnBreadBakingStarted();
            }));

        } catch (...) {
            CompleteWithException();
        }
    }

private:
    void OnSausageFryingStarted() {
        sausage_timer_ = std::make_unique<net::steady_timer>(io_, 1500ms);
        sausage_timer_->async_wait(net::bind_executor(strand_, [self = shared_from_this()](const sys::error_code& ec) {
            self->OnSausageReady(ec);
        }));
    }

    void OnBreadBakingStarted() {
        bread_timer_ = std::make_unique<net::steady_timer>(io_, 1000ms);
        bread_timer_->async_wait(net::bind_executor(strand_, [self = shared_from_this()](const sys::error_code& ec) {
            self->OnBreadReady(ec);
        }));
    }

    void OnSausageReady(const sys::error_code& ec) {
        if (ec) { CompleteWithError(ec); return; }
        sausage_->StopFry();
        sausage_ready_ = true;
        CheckAndAssemble();
    }

    void OnBreadReady(const sys::error_code& ec) {
        if (ec) { CompleteWithError(ec); return; }
        bread_->StopBake();
        bread_ready_ = true;
        CheckAndAssemble();
    }

    void CheckAndAssemble() {
        if (sausage_ready_ && bread_ready_ && !is_completed_.exchange(true)) {
            try {
                HotDog hot_dog(order_id_, std::move(sausage_), std::move(bread_));
                handler_(Result<HotDog>(std::move(hot_dog)));
            } catch (...) {
                CompleteWithException();
            }
        }
    }

    void CompleteWithError(const sys::error_code& ec) {
        if (!is_completed_.exchange(true)) {
            handler_(Result<HotDog>(std::make_exception_ptr(sys::system_error(ec))));
        }
    }

    void CompleteWithException() {
        if (!is_completed_.exchange(true)) {
            handler_(Result<HotDog>(std::current_exception()));
        }
    }

    net::io_context& io_;
    Store& store_;
    std::shared_ptr<GasCooker> cooker_;
    int order_id_;
    HotDogHandler handler_;

    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    std::unique_ptr<net::steady_timer> sausage_timer_;
    std::unique_ptr<net::steady_timer> bread_timer_;
    bool sausage_ready_ = false;
    bool bread_ready_ = false;
    std::atomic<bool> is_completed_{false};
};

class Cafeteria {
public:
    using HotDogHandler = std::function<void(Result<HotDog>)>;

    explicit Cafeteria(net::io_context& io)
        : io_(io)
        , store_()
        , gas_cooker_(io) 
    {
        cooker_ptr_ = std::shared_ptr<GasCooker>(&gas_cooker_, [](GasCooker*) {});
    }

    void OrderHotDog(HotDogHandler handler) {
        int order_id = ++next_order_id_;

        // Передаем наш cooker_ptr_, внутри которого скрытый weak_ptr плиты теперь УСПЕШНО заполнен!
        auto order = std::make_shared<OrderContext>(
            io_, store_, cooker_ptr_, order_id, std::move(handler)
        );
        order->Start();
    }

private:
    net::io_context& io_;

    // Оригинальные поля из precode (типы и порядок строго сохранены)
    Store store_;
    GasCooker gas_cooker_;

    // Умный указатель для безопасной передачи плиты в OrderContext
    std::shared_ptr<GasCooker> cooker_ptr_;

    std::atomic<int> next_order_id_{0};
};
