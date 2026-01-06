/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>

#define BT_LE_AD_ONLY_GENERAL 0x06
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define MANUFACTURER_DATA 1
#define COMPANY_ID 0x06D5
#define DEVICE_ID 0xAABB
#define S_ADVT 0
#define S_TYPE 0x0A

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

static const struct device *const sen_scd = DEVICE_DT_GET(DT_ALIAS(co2));
struct scd_data_t
{
	void *lifo_reserved; /* 1st word reserved for use by lifo */
	struct sensor_value co2;
};

K_LIFO_DEFINE(co2_lifo);

void scd(void)
{
	struct sensor_value co2;
	int ret;

	if (!device_is_ready(sen_scd))
	{
		printk("Device %s is not ready.\n", sen_scd->name);
		return;
	}

	while (1)
	{
		
		/*fetch new data*/
		ret = sensor_sample_fetch(sen_scd);
		if (ret < 0)
		{
			printk("failed sample fetch from %s\n", sen_scd->name);
		}

		/*get data*/
		sensor_channel_get(sen_scd, SENSOR_CHAN_CO2, &co2);

		struct scd_data_t tx_data = {.co2 = co2};
		printk("looping scd %d %d\n", co2.val1, co2.val2);

		/*allocate new memory for tx_data*/
		// size_t size = sizeof(struct scd_data_t);
		// char *mem_ptr = k_malloc(size);
		// __ASSERT_NO_MSG(mem_ptr != 0);

		// memcpy(mem_ptr, &tx_data, size);

		/*put tx_data to a lifo*/
		// k_lifo_put(&co2_lifo, mem_ptr);

		k_msleep(6000);
	}
}
int main(void)
{
	printk("Starting SCD4x sensor app\n");
	scd();
	return 0;
}

// K_THREAD_DEFINE(scd_id, STACKSIZE, scd, NULL, NULL, NULL,
// 								PRIORITY, 0, 0);
