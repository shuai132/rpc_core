/**
* 统一控制调试信息
* 为了保证输出顺序 都使用stdout而不是stderr
*
* 可配置项（默认都是未定义）
* RpcCore_LOG_NDEBUG               关闭RpcCore_LOGD的输出
* RpcCore_LOG_SHOW_VERBOSE         显示RpcCore_LOGV的输出
* RpcCore_LOG_DISABLE_COLOR        禁用颜色显示
* RpcCore_LOG_LINE_END_CRLF        默认是\n结尾 添加此宏将以\r\n结尾
* RpcCore_LOG_FOR_MCU              MCU项目可配置此宏 更适用于MCU环境
* RpcCore_LOG_NOT_EXIT_ON_FATAL    FATAL默认退出程序 添加此宏将不退出
*
* 其他配置项
* RpcCore_LOG_PRINTF_IMPL          定义输出实现（默认使用printf）
* 并添加形如int RpcCore_LOG_PRINTF_IMPL(const char *fmt, ...)的实现
*
* 在库中使用时
* 1. 修改此文件中的`RpcCore_LOG`以包含库名前缀（全部替换即可）
* 2. 取消这行注释: #define RpcCore_LOG_IN_LIB
* 库中可配置项
* RpcCore_LOG_SHOW_DEBUG           开启RpcCore_LOGD的输出
*
* 非库中使用时
* RpcCore_LOGD的输出在debug时打开 release时关闭（依据NDEBUG宏）
*/

#pragma once

// clang-format off

// 在库中使用时需取消注释
#define RpcCore_LOG_IN_LIB

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
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

#if defined(_WIN32) || defined(__ANDROID__) || defined(RpcCore_LOG_FOR_MCU)
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

#if __ANDROID__
#include <android/log.h>
#define RpcCore_LOG_PRINTF(...)         __android_log_print(ANDROID_RpcCore_LOG_DEBUG, "RpcCore_LOG", __VA_ARGS__)
#else
#define RpcCore_LOG_PRINTF(...)         printf(__VA_ARGS__)
#endif

#ifndef RpcCore_LOG_PRINTF_IMPL
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif
#define RpcCore_LOG_PRINTF_IMPL(...)    RpcCore_LOG_PRINTF(__VA_ARGS__) // NOLINT(bugprone-lambda-function-name)
#else
extern int RpcCore_LOG_PRINTF_IMPL(const char *fmt, ...);
#endif

#define RpcCore_LOG(fmt, ...)           do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_GREEN "[*]: " fmt RpcCore_LOG_END, ##__VA_ARGS__); } while(0)

#define RpcCore_LOGT(tag, fmt, ...)     do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_BLUE "[" tag "]: " fmt RpcCore_LOG_END, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGI(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_YELLOW "[I]: %s: " fmt RpcCore_LOG_END, RpcCore_LOG_BASE_FILENAME, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGW(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_CARMINE "[W]: %s: %s: %d: " fmt RpcCore_LOG_END, RpcCore_LOG_BASE_FILENAME, __func__, __LINE__, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGE(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_RED "[E]: %s: %s: %d: " fmt RpcCore_LOG_END, RpcCore_LOG_BASE_FILENAME, __func__, __LINE__, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGF(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_CYAN "[!]: %s: %s: %d: " fmt RpcCore_LOG_END, RpcCore_LOG_BASE_FILENAME, __func__, __LINE__, ##__VA_ARGS__); RpcCore_LOG_EXIT_PROGRAM(); } while(0)

#if defined(RpcCore_LOG_IN_LIB) && !defined(RpcCore_LOG_SHOW_DEBUG) && !defined(RpcCore_LOG_NDEBUG)
#define RpcCore_LOG_NDEBUG
#endif

#if defined(NDEBUG) || defined(RpcCore_LOG_NDEBUG)
#define RpcCore_LOGD(fmt, ...)          ((void)0)
#else
#define RpcCore_LOGD(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_DEFAULT "[D]: %s: " fmt RpcCore_LOG_END, RpcCore_LOG_BASE_FILENAME, ##__VA_ARGS__); } while(0)
#endif

#if defined(RpcCore_LOG_SHOW_VERBOSE)
#define RpcCore_LOGV(fmt, ...)          do{ RpcCore_LOG_PRINTF_IMPL(RpcCore_LOG_COLOR_DEFAULT "[V]: %s: " fmt RpcCore_LOG_END, RpcCore_LOG_BASE_FILENAME, ##__VA_ARGS__); } while(0)
#else
#define RpcCore_LOGV(fmt, ...)          ((void)0)
#endif
