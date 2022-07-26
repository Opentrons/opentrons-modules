#pragma once

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#ifdef ENABLE_LOGGING

#include <stdarg.h>

/**
 * Call back to get the current task name.
 */
typedef const char* (*logging_task_name_get)();

/**
 * Initialize logging
 * @param app_name  Name of the application.
 * @param task_getter Callback to get the current task name.
 */
void log_init(const char* app_name, logging_task_name_get task_getter);

/**
 * Log message function. Not to be used directly. Use LOG macro.
 */
void log_message(const char* format, ...);

#define LOG(format, ...) log_message(format, ##__VA_ARGS__)

#define LOG_INIT(name, task_getter) log_init(name, task_getter)

#else

#define LOG(...)
#define LOG_INIT(...)

#endif

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
