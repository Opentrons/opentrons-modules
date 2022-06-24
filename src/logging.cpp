#ifdef ENABLE_LOGGING

#include "ot_utils/core/logging.h"

#include <stdio.h>

#include <boost/asio.hpp>

#include "boost/date_time/posix_time/posix_time.hpp"

struct FormatSpecs {
    std::string app_name{};
    logging_task_name_get task_name_getter{nullptr};
};

static auto format_specs = FormatSpecs{};

/**
 * Initialize logging
 * @param app_name  Name of the application.
 * @param task_getter Callback to get the current task name.
 */
void log_init(const char* app_name, logging_task_name_get task_getter) {
    format_specs.app_name = app_name;
    format_specs.task_name_getter = task_getter;
}

void log_message(const char* format, ...) {
    va_list argp;
    va_start(argp, format);

    char buff[256];

    auto sigblock = boost::asio::detail::posix_signal_blocker{};
    vsnprintf(buff, sizeof(buff), format, argp);

    const char* task_name = format_specs.task_name_getter
                                ? format_specs.task_name_getter()
                                : "none";

    using namespace boost::posix_time;

    auto time = microsec_clock::local_time();

    printf("[%s] [%s] [%s] %s\n", to_iso_extended_string(time).c_str(),
           format_specs.app_name.c_str(), task_name, buff);

    va_end(argp);
}

#endif
