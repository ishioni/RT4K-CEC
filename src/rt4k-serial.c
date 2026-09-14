#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bsp/board.h"
#include "bsp/board_api.h"
#include "class/cdc/cdc_host.h"
#include "pico/time.h"
#include "tusb.h"

#include "debug.h"
#include "rt4k-serial.h"

#define RT4K_CDC_INDEX 0
#define RT4K_DOUBLE_PRESS_US 1250000u

static const char *const rt4k_commands[0x53] = {
    [0x0A] = "remote gain\n",
    [0x13] = "remote phase\n",
    [0x28] = "remote ok\n",
    [0x45] = "remote back\n",
    [0x4F] = "remote right\n",
    [0x50] = "remote left\n",
    [0x51] = "remote down\n",
    [0x52] = "remote up\n",
};

static const char *const rt4k_power_commands[] = {
    "remote pwr\n",
    "pwr on\n",
};

static bool rt4k_serial_write(const char *command) {
  if (!tuh_cdc_mounted(RT4K_CDC_INDEX)) {
    debug_puts("RT4K serial: command dropped; device is not mounted\r\n");
    return false;
  }

  uint32_t length = (uint32_t)strlen(command);
  if (tuh_cdc_write_available(RT4K_CDC_INDEX) < length) {
    debug_puts("RT4K serial: command did not fit in TX buffer\r\n");
    return false;
  }

  uint32_t written = tuh_cdc_write(RT4K_CDC_INDEX, command, length);
  if (written != length) {
    debug_puts("RT4K serial: command was only partially buffered\r\n");
    tuh_cdc_write_clear(RT4K_CDC_INDEX);
    return false;
  }

  tuh_cdc_write_flush(RT4K_CDC_INDEX);
  debug_printf("RT4K TX: %s", command);
  return true;
}

void rt4k_serial_host_task(void *param) {
  (void)param;

  tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_AUTO,
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  board_init_after_tusb();

  while (true) {
    tuh_task();
  }
}

void rt4k_serial_task(void *param) {
  QueueHandle_t *q = (QueueHandle_t *)param;
  uint8_t key = 0;
  uint8_t last_key = 0;
  uint32_t last_press_time = 0;
  uint8_t power_state = 1;

  while (true) {
    if (xQueueReceive(*q, &key, pdMS_TO_TICKS(10)) != pdTRUE) {
      continue;
    }

    uint32_t now = to_us_since_boot(get_absolute_time());
    if (key == 0x51 || key == 0x52) {
      if (last_key == key && (uint32_t)(now - last_press_time) >= RT4K_DOUBLE_PRESS_US) {
        if (key == 0x51) {
          rt4k_serial_write(rt4k_power_commands[power_state]);
          power_state = (uint8_t)!power_state;
        } else {
          rt4k_serial_write("remote menu\n");
        }
        last_key = 0;
        last_press_time = 0;
      } else {
        const char *command = rt4k_commands[key];
        if (command != NULL) {
          rt4k_serial_write(command);
        }
        last_key = key;
        last_press_time = now;
      }
    } else {
      if (key < (sizeof(rt4k_commands) / sizeof(rt4k_commands[0])) && rt4k_commands[key] != NULL) {
        rt4k_serial_write(rt4k_commands[key]);
      }
      last_key = 0;
      last_press_time = 0;
    }
  }
}

void tuh_mount_cb(uint8_t dev_addr) {
  debug_printf("USB device mounted: address=%u\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
  debug_printf("USB device unmounted: address=%u\r\n", dev_addr);
}

void tuh_cdc_mount_cb(uint8_t idx) {
  tuh_itf_info_t info = {0};
  if (!tuh_cdc_itf_get_info(idx, &info)) {
    debug_printf("CDC interface mounted: index=%u\r\n", idx);
    return;
  }

  cdc_line_coding_t coding = {0};
  tuh_cdc_get_line_coding_local(idx, &coding);
  debug_printf("RT4K serial mounted: index=%u address=%u interface=%u baud=%" PRIu32
               " format=%u%c%s\r\n",
               idx, info.daddr, info.desc.bInterfaceNumber, coding.bit_rate, coding.data_bits,
               CDC_LINE_CODING_PARITY_CHAR(coding.parity), CDC_LINE_CODING_STOP_BITS_TEXT(coding.stop_bits));
}

void tuh_cdc_umount_cb(uint8_t idx) {
  debug_printf("RT4K serial unmounted: index=%u\r\n", idx);
}

void tuh_cdc_rx_cb(uint8_t idx) {
  char buffer[64];

  while (tuh_cdc_read_available(idx) != 0) {
    uint32_t length = tuh_cdc_read(idx, buffer, sizeof(buffer));
    if (length != 0) {
      debug_puts("RT4K RX: ");
      debug_write(buffer, length);
      debug_puts("\r\n");
    }
  }
}
