#pragma once

#include <string_view>
#include <string>
#include <exception>
#include <cstdint>

namespace logger {

void InitLogger();
void LogServerStarted(int port, std::string_view address);
void LogServerStopped(int return_code, const std::exception* ex = nullptr);
void LogRequest(std::string_view ip, std::string_view uri, std::string_view method);
void LogResponse(int64_t response_time_ms, int status_code, std::string_view content_type, bool has_content_type);
void LogNetworkError(int error_code, std::string_view text, std::string_view where);

}  // namespace logger
