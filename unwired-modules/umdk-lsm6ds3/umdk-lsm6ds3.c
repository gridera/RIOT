/*
 * Copyright (C) 2016 Unwired Devices [info@unwds.com]
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup
 * @ingroup
 * @brief
 * @{
 * @file		umdk-lsm6ds3.c
 * @brief       umdk-lsm6ds3 module implementation
 * @author      Eugene Ponomarev
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "periph/gpio.h"

#include "board.h"

#include "unwds-common.h"
#include "include/umdk-lsm6ds3.h"
#include "include/lsm6ds3.h"

#include "thread.h"
#include "xtimer.h"

static uwnds_cb_t *callback;

static lsm6ds3_t lsm6ds3;

static kernel_pid_t acq_pid;
static msg_t acq_msg = {};
static xtimer_t acq_timer;

int acq_ths = 500;

lsm6ds3_data_t acc_max_value;

static void *acq_thread(void *arg) {
    msg_t msg;
    msg_t msg_queue[4];
    msg_init_queue(msg_queue, 4);

    puts("[umdk-sht21] Continuous acquisition thread started");

    while (1) {
        msg_receive(&msg);

        xtimer_remove(&acq_timer);

        lsm6ds3_data_t acc_data = {};
        lsm6ds3_get_raw(&lsm6ds3, &acc_data);
        
        if (acc_data.acc_x > 0) {
            if (acc_data.acc_x > acc_max_value.acc_x) {
                acc_max_value.acc_x = acc_data.acc_x;
            } else {
                if ((acc_data.acc_x + acq_ths) < acc_max_value.acc_x) {
                    printf("Acceleration peak: X %d.%d g\n",
                            acc_max_value.acc_x/1000, abs(acc_max_value.acc_x%1000));
                    acc_max_value.acc_x = INT_MIN;
                }
            }
        }
        
        if (acc_data.acc_y > 0) {
            if (acc_data.acc_y > acc_max_value.acc_y) {
                acc_max_value.acc_y = acc_data.acc_y;
            } else {
                if ((acc_data.acc_y + acq_ths) < acc_max_value.acc_y) {
                    printf("Acceleration peak: Y %d.%d g\n",
                            acc_max_value.acc_y/1000, abs(acc_max_value.acc_y%1000));
                    acc_max_value.acc_y = INT_MIN;
                }
            }
        }
        
        if (acc_data.acc_z > 0) {
            if (acc_data.acc_z > acc_max_value.acc_z) {
                acc_max_value.acc_z = acc_data.acc_z;
            }
             else {
                if ((acc_data.acc_z + acq_ths) < acc_max_value.acc_z) {
                    printf("Acceleration peak: Z %d.%d g\n",
                            acc_max_value.acc_z/1000, abs(acc_max_value.acc_z%1000));
                    acc_max_value.acc_z = INT_MIN;
                }
            }
        }
        
        /* every 10 ms */
        xtimer_set_msg(&acq_timer, 10U * 1000U, &acq_msg, acq_pid);
    }
    return NULL;
}

int umdk_lsm6ds3_shell_cmd(int argc, char **argv) {
    if (argc == 1) {
        puts ("lsm6ds3 get - get results now");
        puts ("lsm6ds3 send - get and send results now");
        puts ("lsm6ds3 start - continuous acquisition with peak detection");
        puts ("lsm6ds3 stop - stop continuous acquisition");
        puts ("lsm6ds3 rate <13|26|52|104|208|416|833|1660> - set sensor data rate");
        puts ("lsm6ds3 ths <value> - accelerometer threshold for peak detection, mg");
        puts ("lsm6ds3 filter <50|100|200|400> - anti-aliasing filter, Hz");
        return 0;
    }
    
    char *cmd = argv[1];
	
    if (strcmp(cmd, "get") == 0) {
        lsm6ds3_data_t acc_data = {};
        lsm6ds3_get_raw(&lsm6ds3, &acc_data);
		uint16_t temp = lsm6ds3_read_temp_c(&lsm6ds3);
        printf("Accelerometer: X %d.%d g, Y %d.%d g, Z %d.%d g\n",
                acc_data.acc_x/1000, abs(acc_data.acc_x%1000),
                acc_data.acc_y/1000, abs(acc_data.acc_y%1000),
                acc_data.acc_z/1000, abs(acc_data.acc_z%1000));
        printf("Gyroscope: X %d.%d dps, Y %d.%d dps, Z %d.%d dps\n",
                acc_data.gyr_x/1000, abs(acc_data.gyr_x%1000),
                acc_data.gyr_y/1000, abs(acc_data.gyr_y%1000),
                acc_data.acc_z/1000, abs(acc_data.gyr_z%1000));
        printf("Temperature: %d.%d C\n", temp/1000, abs(temp%1000));
    }
    
    if (strcmp(cmd, "send") == 0) {
        puts("Not implemented yet");
    }
    
    if (strcmp(cmd, "start") == 0) {
        acc_max_value.acc_x = INT_MIN;
        acc_max_value.acc_y = INT_MIN;
        acc_max_value.acc_z = INT_MIN;
        
        acc_max_value.gyr_x = INT_MIN;
        acc_max_value.gyr_y = INT_MIN;
        acc_max_value.gyr_z = INT_MIN;
        
        xtimer_set_msg(&acq_timer, 10U * 1000U, &acq_msg, acq_pid);
    }
    
    if (strcmp(cmd, "stop") == 0) {
        xtimer_remove(&acq_timer);
    }
    
    if (strcmp(cmd, "ths") == 0) {
        acq_ths = atoi(argv[2]);
        printf("[umdk-lsm6ds3] Threshold set: %d mg\n", acq_ths);
    }
    
    if (strcmp(cmd, "filter") == 0) {
        int filter = atoi(argv[2]);
        switch (filter)
        {
            case 50:
                lsm6ds3.params.accel_bandwidth = LSM6DS3_ACC_GYRO_BW_XL_50Hz;
                break;
            case 100:
                lsm6ds3.params.accel_bandwidth = LSM6DS3_ACC_GYRO_BW_XL_100Hz;
                break;
            case 200:
                lsm6ds3.params.accel_bandwidth = LSM6DS3_ACC_GYRO_BW_XL_200Hz;
                break;
            case 400:
                lsm6ds3.params.accel_bandwidth = LSM6DS3_ACC_GYRO_BW_XL_400Hz;
                break;
            default:
                puts("[umdk-lsm6ds3] Invalid filter value");
        }
        if (lsm6ds3_init(&lsm6ds3) < 0) {
            puts("[umdk-lsm6ds3] LSM6DS3 initialization failed");
            return -1;
        } else {
            printf("[umdk-lsm6ds3] Filter value set: %d Hz\n", filter);
        }
    }
   
    if (strcmp(cmd, "rate") == 0) {
        int rate = atoi(argv[2]);
        switch (rate)
        {
            case 13:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_13Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_13Hz;
                break;
            case 26:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_26Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_26Hz;
                break;
            case 52:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_52Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_52Hz;
                break;
            case 104:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_104Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_104Hz;
                break;
            case 208:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_208Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_208Hz;
                break;
            case 416:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_416Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_416Hz;
                break;
            case 833:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_833Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_833Hz;
                break;
            case 1660:
                lsm6ds3.params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_1660Hz;
                lsm6ds3.params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_1660Hz;
                break;
            default:
                puts("[umdk-lsm6ds3] Invalid sample rate value");
        }
        if (lsm6ds3_init(&lsm6ds3) < 0) {
            puts("[umdk-lsm6ds3] LSM6DS3 initialization failed");
            return -1;
        } else {
            printf("[umdk-lsm6ds3] Sample rate set: %d Hz\n", rate);
        }
    }
    
    return 1;
}

void umdk_lsm6ds3_init(uint32_t *non_gpio_pin_map, uwnds_cb_t *event_callback)
{
    (void) non_gpio_pin_map;

    callback = event_callback;

    lsm6ds3_param_t lsm_params;
    lsm_params.i2c_addr = 0x6A;
    lsm_params.i2c = UMDK_LSM6DS3_I2C;

    /* Configure the default settings */
    lsm_params.gyro_enabled = true;
    lsm_params.gyro_range = LSM6DS3_ACC_GYRO_FS_G_500dps;
    lsm_params.gyro_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_1660Hz;
    lsm_params.gyro_bandwidth = LSM6DS3_ACC_GYRO_BW_XL_400Hz;
    lsm_params.gyro_fifo_enabled = true;
    lsm_params.gyro_fifo_decimation = true;

    lsm_params.accel_enabled = true;
    lsm_params.accel_odr_off = true;
    lsm_params.accel_range = LSM6DS3_ACC_GYRO_FS_XL_16g;
    lsm_params.accel_sample_rate = LSM6DS3_ACC_GYRO_ODR_XL_1660Hz;
    lsm_params.accel_bandwidth = LSM6DS3_ACC_GYRO_BW_XL_400Hz;
    lsm_params.accel_fifo_enabled = true;
    lsm_params.accel_fifo_decimation = true;

    lsm_params.temp_enabled = true;

    lsm_params.comm_mode = 1;

    lsm_params.fifo_threshold = 3000;
    lsm_params.fifo_sample_rate = LSM6DS3_ACC_GYRO_ODR_FIFO_1600Hz;
    lsm_params.fifo_mode_word = 0;

    lsm6ds3.params = lsm_params;
    
    if (lsm6ds3_init(&lsm6ds3) < 0) {
        puts("[umdk-lsm6ds3] LSM6DS3 initialization failed");
        return;
    }
    
    char *stack = (char *) allocate_stack();
	if (!stack) {
		puts("umdk-sht21: unable to allocate memory. Are too many modules enabled?");
		return;
	}
    acq_pid = thread_create(stack, UNWDS_STACK_SIZE_BYTES, THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                            acq_thread, NULL, "LSM6DS3 acquisition");
    
    unwds_add_shell_command("lsm6ds3", "type 'lsm6ds3' for commands list", umdk_lsm6ds3_shell_cmd);
}

bool umdk_lsm6ds3_cmd(module_data_t *cmd, module_data_t *reply)
{
	/* Check for empty command */
	if (cmd->length < 1)
		return false;

	umdk_lsm6ds3_cmd_t c = cmd->data[0];

	switch (c) {
	case UMDK_LSM6DS3_CMD_POLL:
	{
		lsm6ds3_data_t acc_data = {};
        lsm6ds3_get_raw(&lsm6ds3, &acc_data);
		uint16_t temp = lsm6ds3_read_temp_c(&lsm6ds3);
		
		reply->length = 1 + sizeof(lsm6ds3_data_t) + 2;
		reply->data[0] = UNWDS_LSM6DS3_MODULE_ID;
		
		memcpy(reply->data + 1, &acc_data, sizeof(lsm6ds3_data_t));
		memcpy(reply->data + 1 + sizeof(lsm6ds3_data_t), &temp, 2);
		
		break;
	}
	default:
		return false;
	}
	
    return true;
}

#ifdef __cplusplus
}
#endif