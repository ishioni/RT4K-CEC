#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>

void debug_init(void);
void debug_write(const char *data, size_t length);
void debug_puts(const char *str);
void debug_printf(const char *fmt, ...);

#endif
