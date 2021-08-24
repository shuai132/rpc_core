/**
 * 方便打印日志
 * 为了保证输出顺序 都使用stdout而不是stderr
 *
 * 可配置项（默认都是未定义）
 * LOG_LINE_END_CRLF        默认是\n结尾 添加此宏将以\r\n结尾
 * LOG_FOR_MCU              MCU项目可配置此宏 更适用于MCU环境
 * LOG_NDEBUG               关闭LOGD的输出
 * LOG_SHOW_VERBOSE         显示LOGV的输出
 * LOG_NOT_EXIT_ON_FATAL    FATAL默认退出程序 添加此宏将不退出
 *
 * 其他配置项
 * LOG_PRINTF_IMPL          定义输出实现（默认使用printf库）
 * 并添加形如int LOG_PRINTF_IMPL(const char *fmt, ...)的实现
 */

#pragma once

#ifdef __cplusplus
#include <cstring>
#include <cstdlib>
#else
#include <string.h>
#include <stdlib.h>
#endif

#ifdef  LOG_LINE_END_CRLF
#define LOG_LINE_END        "\r\n"
#else
#define LOG_LINE_END        "\n"
#endif

#ifdef LOG_NOT_EXIT_ON_FATAL
#define LOG_EXIT_PROGRAM()
#else
#ifdef LOG_FOR_MCU
#define LOG_EXIT_PROGRAM()      do{ for(;;); } while(0)
#else
#define LOG_EXIT_PROGRAM()      exit(EXIT_FAILURE)
#endif
#endif

#define LOG_BASE_FILENAME \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
    strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define LOG_WITH_COLOR
#if defined(_WIN32) || defined(__ANDROID__) || defined(LOG_FOR_MCU)
#undef LOG_WITH_COLOR
#endif

#ifdef LOG_WITH_COLOR
#define LOG_COLOR_RED       "\033[31m"
#define LOG_COLOR_GREEN     "\033[32m"
#define LOG_COLOR_YELLOW    "\033[33m"
#define LOG_COLOR_BLUE      "\033[34m"
#define LOG_COLOR_CARMINE   "\033[35m"
#define LOG_COLOR_CYAN      "\033[36m"
#define LOG_COLOR_DEFAULT
#define LOG_COLOR_END       "\033[m"
#else
#define LOG_COLOR_RED
#define LOG_COLOR_GREEN
#define LOG_COLOR_YELLOW
#define LOG_COLOR_BLUE
#define LOG_COLOR_CARMINE
#define LOG_COLOR_CYAN
#define LOG_COLOR_DEFAULT
#define LOG_COLOR_END
#endif

#define LOG_END                 LOG_COLOR_END LOG_LINE_END

#if __ANDROID__
#include <android/log.h>
#define LOG_PRINTF(...)         __android_log_print(ANDROID_LOG_DEBUG, "LOG", __VA_ARGS__)
#else
#define LOG_PRINTF(...)         printf(__VA_ARGS__)
#endif

#ifndef LOG_PRINTF_IMPL
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif
#define LOG_PRINTF_IMPL(...)    LOG_PRINTF(__VA_ARGS__) // NOLINT(bugprone-lambda-function-name)
#else
extern int LOG_PRINTF_IMPL(const char *fmt, ...);
#endif

#define LOG(fmt, ...)           do{ LOG_PRINTF_IMPL(LOG_COLOR_GREEN "[G]: " fmt LOG_END, ##__VA_ARGS__); } while(0)

#define LOGT(tag, fmt, ...)     do{ LOG_PRINTF_IMPL(LOG_COLOR_BLUE "[" tag "]: " fmt LOG_END, ##__VA_ARGS__); } while(0)
#define LOGI(fmt, ...)          do{ LOG_PRINTF_IMPL(LOG_COLOR_YELLOW "[I]: %s: " fmt LOG_END, LOG_BASE_FILENAME, ##__VA_ARGS__); } while(0)
#define LOGW(fmt, ...)          do{ LOG_PRINTF_IMPL(LOG_COLOR_CARMINE "[W]: %s: %s: %d: " fmt LOG_END, \
                                    LOG_BASE_FILENAME, __func__, __LINE__, ##__VA_ARGS__); \
                                } while(0)
#define LOGE(fmt, ...)          do{ LOG_PRINTF_IMPL(LOG_COLOR_RED "[E]: %s: %s: %d: " fmt LOG_END, \
                                    LOG_BASE_FILENAME, __func__, __LINE__, ##__VA_ARGS__); \
                                } while(0)
#define FATAL(fmt, ...)         do{ LOG_PRINTF_IMPL(LOG_COLOR_CYAN "[F]: %s: %s: %d: " fmt LOG_END, \
                                    LOG_BASE_FILENAME, __func__, __LINE__, ##__VA_ARGS__); \
                                    LOG_EXIT_PROGRAM(); \
                                } while(0)

#if defined(NDEBUG) || defined(LOG_NDEBUG)
#define LOGD(fmt, ...)          ((void)0)
#else
#define LOGD(fmt, ...)          do{ LOG_PRINTF_IMPL(LOG_COLOR_DEFAULT "[D]: %s: " fmt LOG_END, LOG_BASE_FILENAME, ##__VA_ARGS__); } while(0)
#endif

#if defined(LOG_SHOW_VERBOSE)
#define LOGV(fmt, ...)          do{ LOG_PRINTF_IMPL(LOG_COLOR_DEFAULT "[V]: %s: " fmt LOG_END, LOG_BASE_FILENAME, ##__VA_ARGS__); } while(0)
#else
#define LOGV(fmt, ...)          ((void)0)
#endif
