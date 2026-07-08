#include "sdk.h"
#include "logger.h"

#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

namespace logger {

const std::string attribute_name = "AdditionalData";

void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    boost::json::object log_object;

    auto timestamp = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    if (timestamp) {
        log_object["timestamp"] = boost::posix_time::to_iso_extended_string(timestamp.get());
    }
    log_object["message"] = *rec[expr::smessage];

    auto attrs = rec.attribute_values();
    auto it = attrs.find(attribute_name);
    if (it != attrs.end()) {
        log_object["data"] = it->second.extract<boost::json::value>().get();
    } else {
        log_object["data"] = boost::json::object{};
    }

    strm << boost::json::serialize(log_object) << std::endl;
}

void InitLogger() {
    logging::add_common_attributes();
    auto console_sink = logging::add_console_log(std::cout, keywords::auto_flush = true);
    console_sink->set_formatter(&JsonFormatter);
}

void LogServerStarted(int port, std::string_view address) {
    boost::json::object data;
    data["port"] = port;
    data["address"] = std::string(address);

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(attribute_name, boost::json::value(data))
        << "server started";
}

void LogServerStopped(int return_code, const std::exception* ex) {
    boost::json::object data;
    data["code"] = return_code;
    if (ex) {
        data["exception"] = std::string(ex->what());
    }

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(attribute_name, boost::json::value(data))
        << "server exited";
}

void LogRequest(std::string_view ip, std::string_view uri, std::string_view method) {
    boost::json::object data;
    data["ip"] = std::string(ip);
    data["URI"] = std::string(uri);
    data["method"] = std::string(method);

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(attribute_name, boost::json::value(data))
        << "request received";
}

void LogResponse(int64_t response_time_ms, int status_code, std::string_view content_type, bool has_content_type) {
    boost::json::object data;
    data["response_time"] = response_time_ms;
    data["code"] = status_code;

    if (has_content_type) {
        data["content_type"] = std::string(content_type);
    } else {
        data["content_type"] = nullptr;
    }

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(attribute_name, boost::json::value(data))
        << "response sent";
}

void LogNetworkError(int error_code, std::string_view text, std::string_view where) {
    boost::json::object data;
    data["code"] = error_code;
    data["text"] = std::string(text);
    data["where"] = std::string(where);

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(attribute_name, boost::json::value(data))
        << "error";
}

}  // namespace logger
