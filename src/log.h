/**
 * 统一控制调试信息
 * 为了保证输出顺序 都使用stdout而不是stderr
 *
 * 可配置项（默认都是未定义）
 * RpcCore_LOG_LINE_END_CRLF        默认是\n结尾 添加此宏将以\r\n结尾
 * RpcCore_LOG_SHOW_DEBUG           开启LOGD的输出
 * RpcCore_LOG_SHOW_VERBOSE         显示LOGV的输出
 */

#pragma once

#include <stdio.h>

#ifdef  RpcCore_LOG_LINE_END_CRLF
#define RpcCore_LOG_LINE_END        "\r\n"
#else
#define RpcCore_LOG_LINE_END        "\n"
#endif

#ifdef RpcCore_LOG_NOT_EXIT_ON_FATAL
#define RpcCore_LOG_EXIT_PROGRAM()
#else
#ifdef RpcCore_LOG_FOR_MCU
#define RpcCore_LOG_EXIT_PROGRAM()  do{ for(;;); } while(0)
#else
#define RpcCore_LOG_EXIT_PROGRAM()  exit(EXIT_FAILURE)
#endif
#endif

#ifdef RpcCore_LOG_WITH_COLOR
#define RpcCore_LOG_COLOR_RED       "\033[31m"
#define RpcCore_LOG_COLOR_GREEN     "\033[32m"
#define RpcCore_LOG_COLOR_YELLOW    "\033[33m"
#define RpcCore_LOG_COLOR_BLUE      "\033[34m"
#define RpcCore_LOG_COLOR_CARMINE   "\033[35m"
#define RpcCore_LOG_COLOR_CYAN      "\033[36m"
#define RpcCore_LOG_COLOR_END       "\033[m"
#else
#define RpcCore_LOG_COLOR_RED
#define RpcCore_LOG_COLOR_GREEN
#define RpcCore_LOG_COLOR_YELLOW
#define RpcCore_LOG_COLOR_BLUE
#define RpcCore_LOG_COLOR_CARMINE
#define RpcCore_LOG_COLOR_CYAN
#define RpcCore_LOG_COLOR_END
#endif

#define RpcCore_LOG_END             RpcCore_LOG_COLOR_END RpcCore_LOG_LINE_END

#ifdef __ANDROID__
#include <android/log.h>
#define RpcCore_LOG_PRINTF(...)     __android_log_print(ANDROID_LOG_DEBUG, "LOG", __VA_ARGS__)
#else
#define RpcCore_LOG_PRINTF(...)     printf(__VA_ARGS__)
#endif

#if defined(RpcCore_LOG_SHOW_VERBOSE)
#define RpcCore_LOGV(fmt, ...)      do{ RpcCore_LOG_PRINTF("[V]: " fmt RpcCore_LOG_LINE_END, ##__VA_ARGS__); } while(0)
#else
#define RpcCore_LOGV(fmt, ...)      ((void)0)
#endif

#if defined(RpcCore_LOG_SHOW_DEBUG)
#define RpcCore_LOGD(fmt, ...)      do{ RpcCore_LOG_PRINTF("[D]: " fmt RpcCore_LOG_LINE_END, ##__VA_ARGS__); } while(0)
#else
#define RpcCore_LOGD(fmt, ...)      ((void)0)
#endif

#define RpcCore_LOG(fmt, ...)       do{ RpcCore_LOG_PRINTF(RpcCore_LOG_COLOR_GREEN fmt RpcCore_LOG_END, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGT(tag, fmt, ...) do{ RpcCore_LOG_PRINTF(RpcCore_LOG_COLOR_BLUE "[" tag "]: " fmt RpcCore_LOG_END, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGI(fmt, ...)      do{ RpcCore_LOG_PRINTF(RpcCore_LOG_COLOR_YELLOW "[I]: " fmt RpcCore_LOG_END, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGW(fmt, ...)      do{ RpcCore_LOG_PRINTF(RpcCore_LOG_COLOR_CARMINE "[W]: %s: %d: " fmt RpcCore_LOG_END, __func__, __LINE__, ##__VA_ARGS__); } while(0)
#define RpcCore_LOGE(fmt, ...)      do{ RpcCore_LOG_PRINTF(RpcCore_LOG_COLOR_RED "[E]: %s: %d: " fmt RpcCore_LOG_END, __func__, __LINE__, ##__VA_ARGS__); } while(0)
