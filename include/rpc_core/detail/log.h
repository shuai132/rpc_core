// 1. global control
// L_O_G_NDEBUG                 disable debug log(auto by NDEBUG)
// L_O_G_SHOW_DEBUG             force enable debug log
// L_O_G_DISABLE_ALL            force disable all log
// L_O_G_DISABLE_COLOR          disable color
// L_O_G_LINE_END_CRLF
// L_O_G_SHOW_FULL_PATH
// L_O_G_FOR_MCU
// L_O_G_FREERTOS
// L_O_G_NOT_EXIT_ON_FATAL
//
// C++11 enable default:
// L_O_G_ENABLE_THREAD_SAFE     thread safety
// L_O_G_ENABLE_THREAD_ID       show thread id
// L_O_G_ENABLE_DATE_TIME       show data time
// can disable by define:
// L_O_G_DISABLE_THREAD_SAFE
// L_O_G_DISABLE_THREAD_ID
// L_O_G_DISABLE_DATE_TIME
//
// 2. custom implements
// L_O_G_PRINTF_CUSTOM          int L_O_G_PRINTF_CUSTOM(const char *fmt, ...)
// L_O_G_GET_TID_CUSTOM         uint32_t L_O_G_GET_TID_CUSTOM()
//
// 3. use in library
// 3.1. rename `RPC_CORE_LOG` to library name
// 3.2. define `RPC_CORE_LOG_IN_LIB`
// 3.3. configuration options
// RPC_CORE_LOG_SHOW_DEBUG
// RPC_CORE_LOG_SHOW_VERBOSE
// RPC_CORE_LOG_DISABLE_ALL

#pragma once

// clang-format off

#define RPC_CORE_LOG_IN_LIB

#if defined(RPC_CORE_LOG_DISABLE_ALL) || defined(L_O_G_DISABLE_ALL)

#define RPC_CORE_LOG(fmt, ...)           ((void)0)
#define RPC_CORE_LOGT(tag, fmt, ...)     ((void)0)
#define RPC_CORE_LOGI(fmt, ...)          ((void)0)
#define RPC_CORE_LOGW(fmt, ...)          ((void)0)
#define RPC_CORE_LOGE(fmt, ...)          ((void)0)
#define RPC_CORE_LOGF(fmt, ...)          ((void)0)
#define RPC_CORE_LOGD(fmt, ...)          ((void)0)
#define RPC_CORE_LOGV(fmt, ...)          ((void)0)

#else

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#if __cplusplus >= 201103L || defined(_MSC_VER)

#if !defined(L_O_G_DISABLE_THREAD_SAFE) && !defined(L_O_G_ENABLE_THREAD_SAFE)
#define L_O_G_ENABLE_THREAD_SAFE
#endif

#if !defined(L_O_G_DISABLE_THREAD_ID) && !defined(L_O_G_ENABLE_THREAD_ID)
#define L_O_G_ENABLE_THREAD_ID
#endif

#if !defined(L_O_G_DISABLE_DATE_TIME) && !defined(L_O_G_ENABLE_DATE_TIME)
#define L_O_G_ENABLE_DATE_TIME
#endif

#endif
#else
#include <string.h>
#include <stdlib.h>
#endif

#ifdef  L_O_G_LINE_END_CRLF
#define RPC_CORE_LOG_LINE_END            "\r\n"
#else
#define RPC_CORE_LOG_LINE_END            "\n"
#endif

#ifdef L_O_G_NOT_EXIT_ON_FATAL
#define RPC_CORE_LOG_EXIT_PROGRAM()
#else
#ifdef L_O_G_FOR_MCU
#define RPC_CORE_LOG_EXIT_PROGRAM()      do{ for(;;); } while(0)
#else
#define RPC_CORE_LOG_EXIT_PROGRAM()      exit(EXIT_FAILURE)
#endif
#endif

#ifdef L_O_G_SHOW_FULL_PATH
#define RPC_CORE_LOG_BASE_FILENAME       (__FILE__)
#else
#ifdef __FILE_NAME__
#define RPC_CORE_LOG_BASE_FILENAME       (__FILE_NAME__)
#else
#define RPC_CORE_LOG_BASE_FILENAME       (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif
#endif

#define RPC_CORE_LOG_WITH_COLOR

#if defined(_WIN32) || (defined(__ANDROID__) && !defined(ANDROID_STANDALONE)) || defined(L_O_G_FOR_MCU) || defined(ESP_PLATFORM)
#undef RPC_CORE_LOG_WITH_COLOR
#endif

#ifdef L_O_G_DISABLE_COLOR
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

#ifndef L_O_G_PRINTF
#ifndef RPC_CORE_LOG_PRINTF_DEFAULT
#if defined(__ANDROID__) && !defined(ANDROID_STANDALONE)
#include <android/log.h>
#define RPC_CORE_LOG_PRINTF_DEFAULT(fmt, ...) __android_log_print(ANDROID_L##OG_DEBUG, "RPC_CORE_LOG", fmt, ##__VA_ARGS__)
#else
#define RPC_CORE_LOG_PRINTF_DEFAULT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif
#endif

#ifndef L_O_G_PRINTF_CUSTOM
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif
#ifdef L_O_G_ENABLE_THREAD_SAFE
#ifndef L_O_G_NS_MUTEX
#define L_O_G_NS_MUTEX L_O_G_NS_MUTEX
#include <mutex>
// 1. struct instead of namespace, ensure single instance
struct L_O_G_NS_MUTEX {
static std::mutex& mutex() {
  // 2. never delete, avoid destroy before user log
  // 3. static memory, avoid memory fragmentation
  static char memory[sizeof(std::mutex)];
  static std::mutex& mutex = *(new (memory) std::mutex());
  return mutex;
}
};
#endif
#define L_O_G_PRINTF(fmt, ...) { \
  std::lock_guard<std::mutex> L_O_G_NS_lock(L_O_G_NS_MUTEX::mutex()); \
  RPC_CORE_LOG_PRINTF_DEFAULT(fmt, ##__VA_ARGS__); \
}
#else
#define L_O_G_PRINTF(fmt, ...)  RPC_CORE_LOG_PRINTF_DEFAULT(fmt, ##__VA_ARGS__)
#endif
#else
extern int L_O_G_PRINTF_CUSTOM(const char *fmt, ...);
#define L_O_G_PRINTF(fmt, ...)  L_O_G_PRINTF_CUSTOM(fmt, ##__VA_ARGS__)
#endif
#endif

#ifdef L_O_G_ENABLE_THREAD_ID
#ifndef L_O_G_NS_GET_TID
#define L_O_G_NS_GET_TID L_O_G_NS_GET_TID
#include <cstdint>
#ifdef L_O_G_GET_TID_CUSTOM
extern uint32_t L_O_G_GET_TID_CUSTOM();
#elif defined(_WIN32)
#include <Windows.h>
#include <processthreadsapi.h>
struct L_O_G_NS_GET_TID {
static inline uint32_t get_tid() {
  return GetCurrentThreadId();
}
};
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
struct L_O_G_NS_GET_TID {
static inline uint32_t get_tid() {
  return syscall(SYS_gettid);
}
};
#elif defined(L_O_G_FREERTOS) || defined(FREERTOS_CONFIG_H)
#include <freertos/FreeRTOS.h>
struct L_O_G_NS_GET_TID {
static inline uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}
};
#else /* for mac, bsd.. */
#include <pthread.h>
struct L_O_G_NS_GET_TID {
static inline uint32_t get_tid() {
  uint64_t x;
  pthread_threadid_np(nullptr, &x);
  return (uint32_t)x;
}
};
#endif
#endif
#ifdef L_O_G_GET_TID_CUSTOM
#define RPC_CORE_LOG_THREAD_LABEL "%u "
#define RPC_CORE_LOG_THREAD_VALUE ,L_O_G_GET_TID_CUSTOM()
#else
#define RPC_CORE_LOG_THREAD_LABEL "%u "
#define RPC_CORE_LOG_THREAD_VALUE ,L_O_G_NS_GET_TID::get_tid()
#endif
#else
#define RPC_CORE_LOG_THREAD_LABEL
#define RPC_CORE_LOG_THREAD_VALUE
#endif

#ifdef L_O_G_ENABLE_DATE_TIME
#include <chrono>
#include <sstream>
#include <iomanip> // std::put_time
#ifndef L_O_G_NS_GET_TIME
#define L_O_G_NS_GET_TIME L_O_G_NS_GET_TIME
struct L_O_G_NS_GET_TIME {
static inline std::string get_time() {
  auto now = std::chrono::system_clock::now();
  std::time_t time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  std::stringstream ss;
  std::tm dst; // NOLINT
#ifdef _MSC_VER
  ::localtime_s(&dst, &time);
#else
  dst = *std::localtime(&time);
#endif
  ss << std::put_time(&dst, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
  return ss.str();
}
};
#endif
#define RPC_CORE_LOG_TIME_LABEL "%s "
#define RPC_CORE_LOG_TIME_VALUE ,L_O_G_NS_GET_TIME::get_time().c_str()
#else
#define RPC_CORE_LOG_TIME_LABEL
#define RPC_CORE_LOG_TIME_VALUE
#endif

#define RPC_CORE_LOG(fmt, ...)           do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_GREEN   RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[*]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RPC_CORE_LOGT(tag, fmt, ...)     do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_BLUE    RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[" tag "]: %s:%d " fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RPC_CORE_LOGI(fmt, ...)          do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_YELLOW  RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[I]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#define RPC_CORE_LOGW(fmt, ...)          do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_CARMINE RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[W]: %s:%d [%s] "  fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); } while(0)                     // NOLINT(bugprone-lambda-function-name)
#define RPC_CORE_LOGE(fmt, ...)          do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_RED     RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[E]: %s:%d [%s] "  fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); } while(0)                     // NOLINT(bugprone-lambda-function-name)
#define RPC_CORE_LOGF(fmt, ...)          do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_CYAN    RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[!]: %s:%d [%s] "  fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, __func__, ##__VA_ARGS__); RPC_CORE_LOG_EXIT_PROGRAM(); } while(0) // NOLINT(bugprone-lambda-function-name)

#if defined(RPC_CORE_LOG_IN_LIB) && !defined(RPC_CORE_LOG_SHOW_DEBUG) && !defined(L_O_G_NDEBUG)
#define RPC_CORE_LOG_NDEBUG
#endif

#if defined(L_O_G_NDEBUG) && !defined(RPC_CORE_LOG_NDEBUG)
#define RPC_CORE_LOG_NDEBUG
#endif

#if (defined(NDEBUG) || defined(RPC_CORE_LOG_NDEBUG)) && !defined(L_O_G_SHOW_DEBUG)
#define RPC_CORE_LOGD(fmt, ...)          ((void)0)
#else
#define RPC_CORE_LOGD(fmt, ...)          do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_DEFAULT RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[D]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#endif

#if defined(RPC_CORE_LOG_SHOW_VERBOSE)
#define RPC_CORE_LOGV(fmt, ...)          do{ L_O_G_PRINTF(RPC_CORE_LOG_COLOR_DEFAULT RPC_CORE_LOG_TIME_LABEL RPC_CORE_LOG_THREAD_LABEL "[V]: %s:%d "       fmt RPC_CORE_LOG_END RPC_CORE_LOG_TIME_VALUE RPC_CORE_LOG_THREAD_VALUE, RPC_CORE_LOG_BASE_FILENAME, __LINE__, ##__VA_ARGS__); } while(0)
#else
#define RPC_CORE_LOGV(fmt, ...)          ((void)0)
#endif

#endif
