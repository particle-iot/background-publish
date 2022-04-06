#pragma once

typedef void*os_queue_t;
/**
 * Type by which queues are referenced.  For example, a call to xQueueCreate()
 * returns an QueueHandle_t variable that can then be used as a parameter to
 * xQueueSend(), xQueueReceive(), etc.
 */
typedef void * QueueHandle_t;

typedef void os_thread_return_t;
typedef std::function<os_thread_return_t(void)> wiring_thread_fn_t;
typedef uint8_t os_thread_prio_t;