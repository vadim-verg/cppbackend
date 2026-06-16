#include "http_server.h"

namespace http_server {

//------------class SessionBase-----------

void SessionBase::Run() {
    Read();
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(std::chrono::seconds(30));

    http::async_read(
        stream_,
        buffer_,
        request_,
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis())
        );
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        Close();
        return;
    }
    if (ec) {
        ReportError(ec, "read"sv);
        return;
    }
    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        ReportError(ec, "write"sv);
        return;
    }
    if (close) {
        Close();
        return;
    }
    Read();
}

void SessionBase::Close() {
    try {
        stream_.socket().shutdown(tcp::socket::shutdown_send);
    } catch (const boost::system::system_error& e) {
        // Перехватываем исключение и отправляем в лог
        std::clog << "SessionBase::Close(): " << e.what() << "\n";
    }
}

}  // namespace http_server
