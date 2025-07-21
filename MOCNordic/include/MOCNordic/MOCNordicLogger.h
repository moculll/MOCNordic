#pragma once

#include <zephyr/logging/log.h>
namespace MTNordic {
/* test */
namespace MTNordicLogger {


#define DEBUG_PRINT(fmt, ...) \
        do { \
            LOG_INF("[%s.%d] " fmt "\r\n" ,__func__ , __LINE__, ##__VA_ARGS__); \
        } while(0)
#define DEBUG_PRINT_NONNEXT(fmt, ...) \
        do { \
            printk("[%s.%d] " fmt, __func__, __LINE__, ##__VA_ARGS__); \
        } while(0)
#define DEBUG_PRINT_EMPTY(fmt, ...) \
        do { \
            printk(fmt, ##__VA_ARGS__); \
        } while(0)

#define DEBUG_PRINT_HEX(fmt, array_name, array_length) \
        do { \
            LOG_HEXDUMP_INF(array_name, array_length, fmt); \
        } while(0)



} /* MTNordicLogger */


} /* MTNordic */

