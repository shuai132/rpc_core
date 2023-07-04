// 可配置项（默认未定义）
// RpcCore_LOG_NDEBUG               关闭RpcCore_LOGD的输出
// RpcCore_LOG_SHOW_VERBOSE         显示RpcCore_LOGV的输出
// RpcCore_LOG_DISABLE_COLOR        禁用颜色显示
// RpcCore_LOG_LINE_END_CRLF        默认是\n结尾 添加此宏将以\r\n结尾
// RpcCore_LOG_FOR_MCU              更适用于MCU环境
// RpcCore_LOG_NOT_EXIT_ON_FATAL    FATAL默认退出程序 添加此宏将不退出
//
// c++11环境默认打开以下内容
// RpcCore_LOG_ENABLE_THREAD_SAFE   线程安全
// RpcCore_LOG_ENABLE_THREAD_ID     显示线程ID
// RpcCore_LOG_ENABLE_DATE_TIME     显示日期
// 分别可通过下列禁用
// RpcCore_LOG_DISABLE_THREAD_SAFE
// RpcCore_LOG_DISABLE_THREAD_ID
// RpcCore_LOG_DISABLE_DATE_TIME
//
// 其他配置项
// RpcCore_LOG_PRINTF_IMPL          定义输出实现（默认使用printf）
// 并添加形如int RpcCore_LOG_PRINTF_IMPL(const char *fmt, ...)的实现
//
// 在库中使用时
// 1. 修改此文件中的`RpcCore_LOG`以包含库名前缀（全部替换即可）
// 2. 取消这行注释: #define RpcCore_LOG_IN_LIB
// 库中可配置项
// RpcCore_LOG_SHOW_DEBUG           开启RpcCore_LOGD的输出
//
// 非库中使用时
// RpcCore_LOGD的输出在debug时打开 release时关闭（依据NDEBUG宏）

#pragma once

// clang-format off

// 自定义配置
//#include "log_config.h"

// 在库中使用时需取消注释
#define RpcCore_LOG_IN_LIB

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#if __cplusplus >= 201103L

#if !defined(RpcCore_LOG_DISABLE_THREAD_SAFE) && !defined(RpcCore_LOG_ENABLE_THREAD_SAFE)
#define RpcCore_LOG_ENABLE_THREAD_SAFE
#endif

#if !defined(RpcCore_LOG_DISABLE_THREAD_ID) && !defined(RpcCore_LOG_ENABLE_THREAD_ID)
#define RpcCore_LOG_ENABLE_THREAD_ID
#endif

#if !defined(RpcCore_LOG_DISABLE_DATE_TIME) && !defined(RpcCore_LOG_ENABLE_DATE_TIME)
#define RpcCore_LOG_ENABLE_DATE_TIME
#endif

#endif
#else
#include <string.h>
#include <stdlib.h>
#endif

#ifdef  RpcCore_LOG_LINE_END_CRLF
#define RpcCore_LOG_LINE_END            "\r\n"
#else
#define RpcCore_LOG_LINE_END            "\n"
#endif

#ifdef RpcCore_LOG_NOT_EXIT_ON_FATAL
#define RpcCore_LOG_EXIT_PROGRAM()
#else
#ifdef RpcCore_LOG_FOR_MCU
#define RpcCore_LOG_EXIT_PROGRAM()      do{ for(;;); } while(0)
#else
#define RpcCore_LOG_EXIT_PROGRAM()      exit(EXIT_FAILURE)
#endif
#endif

#define RpcCore_LOG_BASE_FILENAME       (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define RpcCore_LOG_WITH_COLOR

#if defined(_WIN32) || (defined(__ANDROID__) && !defined(ANDROID_STANDALONE)) || defined(RpcCore_LOG_FOR_MCU)
#undef RpcCore_LOG_WITH_COLOR
#endif

#ifdef RpcCore_LOG_DISABLE_COLOR
#undef RpcCore_LOG_WITH_COLOR
#endif

#ifdef RpcCore_LOG_WITH_COLOR
#define RpcCore_LOG_COLOR_RED           "\033[31m"
#define RpcCore_LOG_COLOR_GREEN         "\033[32m"
#define RpcCore_LOG_COLOR_YELLOW        "\033[33m"
#define RpcCore_LOG_COLOR_BLUE          "\033[34m"
#define RpcCore_LOG_COLOR_CARMINE       "\033[35m"
#define RpcCore_LOG_COLOR_CYAN          "\033[36m"
#define RpcCore_LOG_COLOR_DEFAULT
#define RpcCore_LOG_COLOR_END           "\033[m"
#else
#define RpcCore_LOG_COLOR_RED
#define RpcCore_LOG_COLOR_GREEN
#define RpcCore_LOG_COLOR_YELLOW
#define RpcCore_LOG_COLOR_BLUE
#define RpcCore_LOG_COLOR_CARMINE
#define RpcCore_LOG_COLOR_CYAN
#define RpcCore_LOG_COLOR_DEFAULT
#define RpcCore_LOG_COLOR_END
#endif

#define RpcCore_LOG_END                 RpcCore_LOG_COLOR_END RpcCore_LOG_LINE_END

#if defined(__ANDROID__) && !defined(ANDROID_STANDALONE)
#include <android/log.h>
#define RpcCore_LOG_PRINTF(...)         __android_log_print(ANDROID_L##OG_DEBUG, "RpcCore_LOG", __VA_ARGS__)
#else
#define RpcCore_LOG_PRINTF(...)         printf(__VA_ARGS__)
#endif

#ifndef RpcCore_LOG_PRINTF_IMPL
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif

#ifdef RpcCore_LOG_ENABLE_THREAD_SAFE
#include <mutex>
struct RpcCore_LOG_Mutex {
static std::mutex& mutex() {
static std::mutex mutex;
return mutex;
}
};
#define RpcCore_LOG_PRINTF_IMPL(...)    \
std::lock_guard<std::mutex> lock(RpcCore_LOG_Mutex::mutex()); \
RpcCore_LOG_PRINTF(__VA_ARGS__)
#else
#define RpcCore_LOG_PRINTF_IMPL(...)    RpcCore_LOG_PRINTF(__VA_ARGS__)
#endif

#else
extern int RpcCore_LOG_PRINTF_IMPL(const char *fmt, ...);
#endif

#ifdef RpcCore_LOG_ENABLE_THREAD_ID
#include <thread>
#include <sstream>
#include <string>
namespace RpcCore_LOG {
inline std::string get_thread_id() {
std::stringstream ss;
ss << std::this_thread::get_id();
return ss.str();
}
}
#define RpcCore_LOG_THREAD_LABEL "%s "
#define RpcCore_LOG_THREAD_VALUE ,RpcCore_LOG::get_thread_id().c_str()
#else
#define RpcCore_LOG_THREAD_LABEL
#define RpcCore_LOG_THREAD_VALUE
#endif

#ifdef RpcCore_LOG_ENABLE_DATE_TIME
#include <chrono>
#include <sstream>
#include <iomanip>
namespace RpcCore_LOG {
inline std::string get_time() {
auto now = std::chrono::system_clock::now();
std::time_t time = std::chrono::system_clock::to_time_t(now);
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
std::stringstream ss;
ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
return ss.str();
}
}
#define RpcCore_LOG_TIME_LABEL "%s "
#define RpcCore_LOG_TIME_VALUE ,RpcCore_LOG::get_time().c_str()
#else
#define RpcCore_LOG_TIME_LABEL
#define RpcCore_LOG_TIME_VALUE
#endif

#define RpcCore_LOG(fmt, ...)           do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_GREEN   RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[*]: %s:%d "       fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGT(tag, fmt, ...)     do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_BLUE    RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[" tag "]: %s:%d " fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGI(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_YELLOW  RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[I]: %s:%d "       fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGW(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_CARMINE RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[W]: %s:%d [%s] "  fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); } while(0)                     // NOLINT(bugprone-lambda-function-name)
#define RpcCore_LOGE(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_RED     RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[E]: %s:%d [%s] "  fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); } while(0)                     // NOLINT(bugprone-lambda-function-name)
#define RpcCore_LOGF(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_CYAN    RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[!]: %s:%d [%s] "  fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); RpcCore_LOG_EXIT_PROGRAM(); } while(0) // NOLINT(bugprone-lambda-function-name)

#if defined(RpcCore_LOG_IN_LIB) && !defined(RpcCore_LOG_SHOW_DEBUG) && !defined(RpcCore_LOG_NDEBUG)
#define RpcCore_LOG_NDEBUG
#endif

#if defined(NDEBUG) || defined(RpcCore_LOG_NDEBUG)
#define RpcCore_LOGD(fmt, ...)          ((void)0)
#else
#define RpcCore_LOGD(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_DEFAULT RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[D]: %s:%d "       fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#endif

#if defined(RpcCore_LOG_SHOW_VERBOSE)
#define RpcCore_LOGV(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_DEFAULT RpcCore_LOG_TIME_LABEL RpcCore_LOG_THREAD_LABEL "[V]: %s:%d "       fmt RpcCore_LOG_END RpcCore_LOG_TIME_VALUE RpcCore_LOG_THREAD_VALUE, RpcCore_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#else
#define RpcCore_LOGV(fmt, ...)          ((void)0)
#endif
