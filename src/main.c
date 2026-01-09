#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/drivers/sensor/scd4x.h>

static const struct device *const sen_scd = DEVICE_DT_GET(DT_ALIAS(scd41));
static const struct device *const sen_bme = DEVICE_DT_GET(DT_ALIAS(bme280));

int round_to_integer(double number) {
	int result = (int)(number + (number >= 0 ? 0.5 : -0.5));
	return result;
}

void start_co2_measurement(void)
{
	struct sensor_value co2_measurement;
	struct sensor_value temperature_measurement;
	struct sensor_value pressure_measurement;
	struct sensor_value humidity_measurement;

	int ret;

	if (!device_is_ready(sen_scd))
	{
		printk("SCD41 %s is not ready.\n", sen_scd->name);
		return;
	}

	if (!device_is_ready(sen_bme))
	{
		printk("BME280 %s is not ready.\n", sen_bme->name);
		return;
	}

	while (1)
	{
		ret = sensor_sample_fetch(sen_bme);
		if (ret < 0)
		{
			printk("failed sample fetch from %s\n", sen_bme->name);
		}

		sensor_channel_get(sen_bme, SENSOR_CHAN_AMBIENT_TEMP, &temperature_measurement);
		sensor_channel_get(sen_bme, SENSOR_CHAN_HUMIDITY, &humidity_measurement);
		sensor_channel_get(sen_bme, SENSOR_CHAN_PRESS, &pressure_measurement);

		printk("looping bme temp %d %d\n", temperature_measurement.val1, temperature_measurement.val2);
		printk("looping bme hum %d %d\n", humidity_measurement.val1, humidity_measurement.val2);
		printk("looping bme press %d %d\n", pressure_measurement.val1, pressure_measurement.val2);

		// pressure in hecto pascals for correct CO2 measurement
		int pressure_corrector = pressure_measurement.val1 * 10 + round_to_integer(pressure_measurement.val2 / 100000.0);
		printk("pressure corrector %d\n", pressure_corrector);

		struct sensor_value corrector = {
			.val1 = pressure_corrector,
			.val2 = 0,
		};

		sensor_attr_set(sen_scd, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &corrector);

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
	printk("Starting SCD4x sensor app with nRF Connect SDK\n");
	start_co2_measurement();
	return 0;
}
