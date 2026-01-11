#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/drivers/sensor/scd4x.h>

#include "bluetoothExposure.h"

static const struct device *const scd_sensor = DEVICE_DT_GET(DT_ALIAS(scd41));
static const struct device *const bme_sensor = DEVICE_DT_GET(DT_ALIAS(bme280));

int round_to_integer(double number) {
	int result = (int)(number + (number >= 0 ? 0.5 : -0.5));
	return result;
}

void start_measuring(void)
{
	if (!device_is_ready(scd_sensor))
	{
		printk("SCD41 %s is not ready.\n", scd_sensor->name);
		return;
	}

	if (!device_is_ready(bme_sensor))
	{
		printk("BME280 %s is not ready.\n", bme_sensor->name);
		return;
	}
}

int get_measurement_data(
	struct sensor_value *co2_measurement,
	struct sensor_value *temperature_measurement,
	struct sensor_value *pressure_measurement,
	struct sensor_value *humidity_measurement
) {
	int ret;

	ret = sensor_sample_fetch(bme_sensor);

	if (ret < 0)
	{
		printk("failed sample fetch from %s\n", bme_sensor->name);
		return ret;
	}

	sensor_channel_get(bme_sensor, SENSOR_CHAN_AMBIENT_TEMP, temperature_measurement);
	sensor_channel_get(bme_sensor, SENSOR_CHAN_HUMIDITY, humidity_measurement);
	sensor_channel_get(bme_sensor, SENSOR_CHAN_PRESS, pressure_measurement);

	printk("bme temp %d %d\n", temperature_measurement->val1, temperature_measurement->val2);
	printk("bme hum %d %d\n", humidity_measurement->val1, humidity_measurement->val2);
	printk("bme press %d %d\n", pressure_measurement->val1, pressure_measurement->val2);

	// pressure in hecto pascals for correct CO2 measurement
	int pressure_correction = pressure_measurement->val1 * 10 + round_to_integer(pressure_measurement->val2 / 100000.0);

	struct sensor_value correction = {
		.val1 = pressure_correction,
		.val2 = 0,
	};

	sensor_attr_set(scd_sensor, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &correction);

	ret = sensor_sample_fetch(scd_sensor);
	if (ret < 0)
	{
		printk("failed sample fetch from %s\n", scd_sensor->name);
		return ret;
	}

	sensor_channel_get(scd_sensor, SENSOR_CHAN_CO2, co2_measurement);

	printk("scd CO2 %d %d\n", co2_measurement->val1, co2_measurement->val2);
	return 0;
}

int main(void)
{
	printk("Starting SCD4x sensor app with nRF Connect SDK\n");

	struct bt_le_ext_adv *advertisement = NULL;

	struct sensor_value co2_measurement;
	struct sensor_value temperature_measurement;
	struct sensor_value pressure_measurement;
	struct sensor_value humidity_measurement;

	start_measuring();

	do {
		get_measurement_data(
			&co2_measurement,
			&temperature_measurement,
			&pressure_measurement,
			&humidity_measurement
		);

		k_sleep(K_MSEC(30));
	} while (co2_measurement.val1 <= 0);

	int16_t temp_bt_home = (temperature_measurement.val1) * 100 + round_to_integer(temperature_measurement.val2 / 10000.0);
	int16_t humidity_bt_home = (humidity_measurement.val1) * 100 + round_to_integer(humidity_measurement.val2 / 10000.0);
	int32_t pressure_bt_home = pressure_measurement.val1 * 1000 + round_to_integer(pressure_measurement.val2 / 1000.0);
	int32_t co2_bt_home = co2_measurement.val1;
	uint8_t battery_charge = 1;

	prepare_bluetooth_advertising(&advertisement);

	update_service_data(
		&advertisement,
		temp_bt_home,
		humidity_bt_home,
		pressure_bt_home,
		co2_bt_home,
		battery_charge
	);

	start_advertising(&advertisement);

	while (1)
	{
		k_sleep(K_MSEC(6000));

		get_measurement_data(
			&co2_measurement,
			&temperature_measurement,
			&pressure_measurement,
			&humidity_measurement
		);

		temp_bt_home = (temperature_measurement.val1) * 100 + round_to_integer(temperature_measurement.val2 / 10000.0);
		humidity_bt_home = (humidity_measurement.val1) * 100 + round_to_integer(humidity_measurement.val2 / 10000.0);
		pressure_bt_home = pressure_measurement.val1 * 1000 + round_to_integer(pressure_measurement.val2 / 1000.0);
		co2_bt_home = co2_measurement.val1;
		battery_charge = 1;

		update_service_data(
			&advertisement,
			temp_bt_home,
			humidity_bt_home,
			pressure_bt_home,
			co2_bt_home,
			battery_charge
		);
	}

	return 0;
}
