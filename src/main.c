#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bsp/board.h"
#include "pico/stdlib.h"

#include "pico-cec/config.h"

#include "blink.h"
#include "cec-frame.h"
#include "cec-log.h"
#include "cec-task.h"
#include "debug.h"
#include "rt4k-serial.h"
#include "ws2812.h"

int main() {
  static StaticQueue_t xCECQueue;
  static uint8_t storageCECQueue[CEC_QUEUE_LENGTH * sizeof(uint8_t)];

  static StackType_t stackLED[LED_STACK_SIZE];
  static StackType_t stackCEC[CEC_STACK_SIZE];
  static StackType_t stackSerial[CDC_STACK_SIZE];
  static StackType_t stackUSB[USB_STACK_SIZE];

  static StaticTask_t xLEDTCB;
  static StaticTask_t xCECTCB;
  static StaticTask_t xUSBTCB;
  static StaticTask_t xSerialTCB;

  static TaskHandle_t xUSBTask;
  static TaskHandle_t xSerialTask;

  blink_init();

  stdio_init_all();
  board_init();
  debug_init();

  alarm_pool_init_default();

  // CEC user-control queue. The RT4K serial task consumes the mapped HID
  // usage values produced by the upstream CEC configuration layer.
  QueueHandle_t cec_q;
  cec_q = xQueueCreateStatic(CEC_QUEUE_LENGTH, sizeof(uint8_t), &storageCECQueue[0], &xCECQueue);

  xBlinkTask = xTaskCreateStatic(blink_task, LED_TASK_NAME, LED_STACK_SIZE, NULL, LED_PRIORITY,
                                 &stackLED[0], &xLEDTCB);
  xCECTask = xTaskCreateStatic(cec_task, CEC_TASK_NAME, CEC_STACK_SIZE, &cec_q, CEC_PRIORITY,
                               &stackCEC[0], &xCECTCB);
  xUSBTask = xTaskCreateStatic(rt4k_serial_host_task, USB_TASK_NAME, USB_STACK_SIZE, NULL, USB_PRIORITY,
                               &stackUSB[0], &xUSBTCB);
  xSerialTask = xTaskCreateStatic(rt4k_serial_task, CDC_TASK_NAME, CDC_STACK_SIZE, &cec_q, CDC_PRIORITY,
                                  &stackSerial[0], &xSerialTCB);

  (void)xBlinkTask;
  (void)xCECTask;
  (void)xUSBTask;
  (void)xSerialTask;

  cec_log_init(debug_puts);
  cec_log_enable();

  vTaskStartScheduler();

  return 0;
}
