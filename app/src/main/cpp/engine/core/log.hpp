#pragma once
// ============================================================
//  Nebula Engine - core/log.hpp
//  Tagged leveled logging. On Android routes to __android_log_print,
//  elsewhere to stderr. Zero-cost-ish, thread-safe enough for tools.
// ============================================================
#include <cstdio>
#include <cstring>
#include <cstdarg>

#if defined(__ANDROID__)
#include <android/log.h>
#define NEBULA_LOG_TAG "NebulaEngine"
#endif

namespace nebula {

enum class LogLevel { Debug = 0, Info, Warn, Error };

inline void logMessage(LogLevel level, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#if defined(__ANDROID__)
    int prio = ANDROID_LOG_INFO;
    switch (level) {
        case LogLevel::Debug: prio = ANDROID_LOG_DEBUG; break;
        case LogLevel::Info:  prio = ANDROID_LOG_INFO;  break;
        case LogLevel::Warn:  prio = ANDROID_LOG_WARN;  break;
        case LogLevel::Error: prio = ANDROID_LOG_ERROR; break;
    }
    __android_log_print(prio, NEBULA_LOG_TAG, "%s", buf);
#else
    const char* tag = (level == LogLevel::Debug) ? "DEBUG" :
                      (level == LogLevel::Info)  ? "INFO " :
                      (level == LogLevel::Warn)  ? "WARN " : "ERROR";
    std::fprintf(stderr, "[Nebula %s] %s\n", tag, buf);
#endif
}

#define NEBULA_LOG_D(...) ::nebula::logMessage(::nebula::LogLevel::Debug, __VA_ARGS__)
#define NEBULA_LOG_I(...) ::nebula::logMessage(::nebula::LogLevel::Info,  __VA_ARGS__)
#define NEBULA_LOG_W(...) ::nebula::logMessage(::nebula::LogLevel::Warn,  __VA_ARGS__)
#define NEBULA_LOG_E(...) ::nebula::logMessage(::nebula::LogLevel::Error, __VA_ARGS__)

} // namespace nebula
