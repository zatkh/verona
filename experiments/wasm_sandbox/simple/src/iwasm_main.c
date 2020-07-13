/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "bh_platform.h"
#include "wasm_export.h"
#include "app_manager_export.h"
#include "runtime_lib.h"
#include "board_config.h"

#include <zephyr.h>
#include <drivers/uart.h>
#include <device.h>

extern void init_sensor_framework();
extern void exit_sensor_framework();
extern int aee_host_msg_callback(void *msg, uint16_t msg_len);

static int uart_char_cnt = 0;

struct device *uart_dev = NULL;

static void uart_irq_callback(struct device *dev)
{
    unsigned char ch;

    while (uart_poll_in(dev, &ch) == 0) {
        uart_char_cnt++;
        aee_host_msg_callback(&ch, 1);
    }
}

static bool host_init()
{
    uart_dev = device_get_binding(HOST_DEVICE_COMM_UART_NAME);
    if (!uart_dev) {
        printf("UART: Device driver not found.\n");
        return false;
    }
    uart_irq_rx_enable(uart_dev);
    uart_irq_callback_set(uart_dev, uart_irq_callback);
    return true;
}

int host_send(void * ctx, const char *buf, int size)
{
    if (!uart_dev)
        return 0;

    for (int i = 0; i < size; i++)
        uart_poll_out(uart_dev, buf[i]);

    return size;
}

void host_destroy()
{
}

static host_interface interface = {
    .init = host_init,
    .send = host_send,
    .destroy = host_destroy
};

static char global_heap_buf[240 * 1024] = { 0 };

int iwasm_main()
{
    RuntimeInitArgs init_args;

    host_init();

    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

    /* initialize runtime environment */
    if (!wasm_runtime_full_init(&init_args)) {
        printf("Init runtime environment failed.\n");
        return -1;
    }

    init_wasm_timer();

    app_manager_startup(&interface);

    wasm_runtime_destroy();
    return -1;
}

