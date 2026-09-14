#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "debug.h"

#define DEBUG_UART uart0
#define DEBUG_UART_BAUD_RATE 115200
#define DEBUG_UART_TX_PIN 0
#define DEBUG_UART_RX_PIN 1

void debug_init(void) {
  uart_init(DEBUG_UART, DEBUG_UART_BAUD_RATE);
  gpio_set_function(DEBUG_UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(DEBUG_UART_RX_PIN, GPIO_FUNC_UART);
}

void debug_write(const char *data, size_t length) {
  if (data == NULL) {
    return;
  }

  for (size_t i = 0; i < length; i++) {
    uart_putc_raw(DEBUG_UART, data[i]);
  }
}

void debug_puts(const char *str) {
  if (str == NULL) {
    return;
  }

  debug_write(str, strlen(str));
}

void debug_printf(const char *fmt, ...) {
  char buffer[192];
  va_list ap;

  va_start(ap, fmt);
  int length = vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  if (length > 0) {
    size_t bytes = (size_t)length;
    if (bytes >= sizeof(buffer)) {
      bytes = sizeof(buffer) - 1;
    }
    debug_write(buffer, bytes);
  }
}
