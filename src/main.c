#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static const struct device *const sen_scd = DEVICE_DT_GET(DT_ALIAS(scd41));

void start_co2_measurement(void)
{
	struct sensor_value co2_measurement;
	int ret;

	if (!device_is_ready(sen_scd))
	{
		printk("Device %s is not ready.\n", sen_scd->name);
		return;
	}

	while (1)
	{
		ret = sensor_sample_fetch(sen_scd);
		if (ret < 0)
		{
			printk("failed sample fetch from %s\n", sen_scd->name);
		}

		sensor_channel_get(sen_scd, SENSOR_CHAN_CO2, &co2_measurement);

		printk("looping scd %d %d\n", co2_measurement.val1, co2_measurement.val2);

		k_msleep(6000);
	}
}
int main(void)
{
	printk("Starting SCD4x sensor app\n");
	start_co2_measurement();
	return 0;
}
