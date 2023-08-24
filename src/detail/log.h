// 可配置项（默认未定义）
// RPC_CORE_LOG_NDEBUG               关闭RPC_CORE_LOGD的输出
// RPC_CORE_LOG_SHOW_VERBOSE         显示RPC_CORE_LOGV的输出
// RPC_CORE_LOG_DISABLE_COLOR        禁用颜色显示
// RPC_CORE_LOG_LINE_END_CRLF        默认是\n结尾 添加此宏将以\r\n结尾
// RPC_CORE_LOG_FOR_MCU              更适用于MCU环境
// RPC_CORE_LOG_NOT_EXIT_ON_FATAL    FATAL默认退出程序 添加此宏将不退出
//
// c++11环境默认打开以下内容
// RPC_CORE_LOG_ENABLE_THREAD_SAFE   线程安全
// RPC_CORE_LOG_ENABLE_THREAD_ID     显示线程ID
// RPC_CORE_LOG_ENABLE_DATE_TIME     显示日期
// 分别可通过下列禁用
// RPC_CORE_LOG_DISABLE_THREAD_SAFE
// RPC_CORE_LOG_DISABLE_THREAD_ID
// RPC_CORE_LOG_DISABLE_DATE_TIME
//
// 其他配置项
// RPC_CORE_LOG_PRINTF_IMPL          定义输出实现（默认使用printf）
// 并添加形如int RPC_CORE_LOG_PRINTF_IMPL(const char *fmt, ...)的实现
//
// 在库中使用时
// 1. 修改此文件中的`RPC_CORE_LOG`以包含库名前缀（全部替换即可）
// 2. 取消这行注释: #define RPC_CORE_LOG_IN_LIB
// 库中可配置项
// RPC_CORE_LOG_SHOW_DEBUG           开启RPC_CORE_LOGD的输出
//
// 非库中使用时
// RPC_CORE_LOGD的输出在debug时打开 release时关闭（依据NDEBUG宏）

#pragma once

// clang-format off

// 自定义配置
//#include "log_config.h"

// 在库中使用时需取消注释
#define RPC_CORE_LOG_IN_LIB

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#if __cplusplus >= 201103L

#if !defined(RPC_CORE_LOG_DISABLE_THREAD_SAFE) && !defined(RPC_CORE_LOG_ENABLE_THREAD_SAFE)
#define RPC_CORE_LOG_ENABLE_THREAD_SAFE
#endif

#if !defined(RPC_CORE_LOG_DISABLE_THREAD_ID) && !defined(RPC_CORE_LOG_ENABLE_THREAD_ID)
#define RPC_CORE_LOG_ENABLE_THREAD_ID
#endif

#if !defined(RPC_CORE_LOG_DISABLE_DATE_TIME) && !defined(RPC_CORE_LOG_ENABLE_DATE_TIME)
#define RPC_CORE_LOG_ENABLE_DATE_TIME
#endif

#endif
#else
#include <string.h>
#include <stdlib.h>
#endif

#ifdef  RPC_CORE_LOG_LINE_END_CRLF
#define RPC_CORE_LOG_LINE_END            "\r\n"
#else
#define RPC_CORE_LOG_LINE_END            "\n"
#endif

#ifdef RPC_CORE_LOG_NOT_EXIT_ON_FATAL
#define RPC_CORE_LOG_EXIT_PROGRAM()
#else
#ifdef RPC_CORE_LOG_FOR_MCU
#define RPC_CORE_LOG_EXIT_PROGRAM()      do{ for(;;); } while(0)
#else
#define RPC_CORE_LOG_EXIT_PROGRAM()      exit(EXIT_FAILURE)
#endif
#endif

#define RPC_CORE_LOG_BASE_FILENAME       (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define RPC_CORE_LOG_WITH_COLOR

#if defined(_WIN32) || (defined(__ANDROID__) && !defined(ANDROID_STANDALONE)) || defined(RPC_CORE_LOG_FOR_MCU)
#undef RPC_CORE_LOG_WITH_COLOR
#endif

#ifdef RPC_CORE_LOG_DISABLE_COLOR
#undef RPC_CORE_LOG_WITH_COLOR
#endif

#ifdef RPC_CORE_LOG_WITH_COLOR
#define RPC_CORE_LOG_COLOR_RED           "\033[31m"
#define RPC_CORE_LOG_COLOR_GREEN         "\033[32m"
#define RPC_CORE_LOG_COLOR_YELLOW        "\033[33m"
#define RPC_CORE_LOG_COLOR_BLUE          "\033[34m"
#define RPC_CORE_LOG_COLOR_CARMINE       "\033[35m"
#define RPC_CORE_LOG_COLOR_CYAN          "\033[36m"
#define RPC_CORE_LOG_COLOR_DEFAULT
#define RPC_CORE_LOG_COLOR_END           "\033[m"
#else
#define RPC_CORE_LOG_COLOR_RED
#define RPC_CORE_LOG_COLOR_GREEN
#define RPC_CORE_LOG_COLOR_YELLOW
#define RPC_CORE_LOG_COLOR_BLUE
#define RPC_CORE_LOG_COLOR_CARMINE
#define RPC_CORE_LOG_COLOR_CYAN
#define RPC_CORE_LOG_COLOR_DEFAULT
#define RPC_CORE_LOG_COLOR_END
#endif

#define RPC_CORE_LOG_END                 RPC_CORE_LOG_COLOR_END RPC_CORE_LOG_LINE_END

#if defined(__ANDROID__) && !defined(ANDROID_STANDALONE)
#include <android/log.h>
#define RPC_CORE_LOG_PRINTF(...)         __android_log_print(ANDROID_L##OG_DEBUG, "RPC_CORE_LOG", __VA_ARGS__)
#else
#define RPC_CORE_LOG_PRINTF(...)         printf(__VA_ARGS__)
#endif

#ifndef RPC_CORE_LOG_PRINTF_IMPL
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif

#ifdef RPC_CORE_LOG_ENABLE_THREAD_SAFE
#include <mutex>
struct RPC_CORE_LOG_Mutex {
static std::mutex& mutex() {
static std::mutex mutex;
return mutex;
}
};
#define RPC_CORE_LOG_PRINTF_IMPL(...)    \
std::lock_guard<std::mutex> lock(RPC_CORE_LOG_Mutex::mutex()); \
RPC_CORE_LOG_PRINTF(__VA_ARGS__)
#else
#define RPC_CORE_LOG_PRINTF_IMPL(...)    RPC_CORE_LOG_PRINTF(__VA_ARGS__)
#endif

#else
extern int RPC_CORE_LOG_PRINTF_IMPL(const char *fmt, ...);
#endif

#ifdef RPC_CORE_LOG_ENABLE_THREAD_ID
#include <thread>
#include <sstream>
#include <string>
namespace RPC_CORE_LOG {
inline std::string get_thread_id() {
std::stringstream ss;
ss << std::this_thread::get_id();
return ss.str();
}
}
#define RPC_CORE_LOG_THREAD_LABEL "%s "
#define RPC_CORE_LOG_THREAD_VALUE ,RPC_CORE_LOG::get_thread_id().c_str()
#else
#define RPC_CORE_LOG_THREAD_LABEL
#define RPC_CORE_LOG_THREAD_VALUE
#endif

#ifdef RPC_CORE_LOG_ENABLE_DATE_TIME
#include <chrono>
#include <sstream>
#include <iomanip>
namespace RPC_CORE_LOG {
inline std::string get_time() {
auto now = std::chrono::system_clock::now();
std::time_t time = std::chrono::system_clock::to_time_t(now);
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
std::stringstream ss;
ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
return ss.str();
}
}
#define RPC_CORE_LOG_TIME_LABEL "%s "
#define RPC_CORE_LOG_TIME_VALUE ,RPC_CORE_LOG::get_time().c_str()
#else
#define RPC_CORE_LOG_TIME_LABEL
#define RPC_CORE_LOG_TIME_VALUE
#endif

#define RPC_CORE_LOG(fmt, ...)           do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_GREEN   RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[*]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RPC_CORE_LOGT(tag, fmt, ...)     do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_BLUE    RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[" tag "]: %s:%d " fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RPC_CORE_LOGI(fmt, ...)          do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_YELLOW  RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[I]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RPC_CORE_LOGW(fmt, ...)          do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_CARMINE RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[W]: %s:%d [%s] "  fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); } while(0)                     // NOLINT(bugprone-lambda-function-name)
#define RPC_CORE_LOGE(fmt, ...)          do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_RED     RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[E]: %s:%d [%s] "  fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); } while(0)                     // NOLINT(bugprone-lambda-function-name)
#define RPC_CORE_LOGF(fmt, ...)          do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_CYAN    RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[!]: %s:%d [%s] "  fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); RPC_CORE_LOG_EXIT_PROGRAM(); } while(0) // NOLINT(bugprone-lambda-function-name)

#if defined(RPC_CORE_LOG_IN_LIB) && !defined(RPC_CORE_LOG_SHOW_DEBUG) && !defined(RPC_CORE_LOG_NDEBUG)
#define RPC_CORE_LOG_NDEBUG
#endif

#if defined(NDEBUG) || defined(RPC_CORE_LOG_NDEBUG)
#define RPC_CORE_LOGD(fmt, ...)          ((void)0)
#else
#define RPC_CORE_LOGD(fmt, ...)          do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_DEFAULT RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[D]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#endif

#if defined(RPC_CORE_LOG_SHOW_VERBOSE)
#define RPC_CORE_LOGV(fmt, ...)          do{ RPC_CORE_LOG_PRINTF_IMPL(RPC_CORE_LOG_COLOR_DEFAULT RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[V]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#else
#define RPC_CORE_LOGV(fmt, ...)          ((void)0)
#endif
