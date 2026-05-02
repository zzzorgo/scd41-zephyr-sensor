#include <zephyr/types.h>
#include <stddef.h>
// #include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor/scd4x.h>

#include "bluetoothExposure.h"
#include "battery.h"
#include "special_boot.h"

static const struct device *const scd_sensor = DEVICE_DT_GET(DT_ALIAS(scd41));
static const struct device *const bme_sensor = DEVICE_DT_GET(DT_ALIAS(bme280));



#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

enum scd4x_model_t {
	SCD4X_MODEL_SCD40,
	SCD4X_MODEL_SCD41,
};

enum scd4x_mode_t {
	SCD4X_MODE_NORMAL,
	SCD4X_MODE_LOW_POWER,
	SCD4X_MODE_SINGLE_SHOT,
};

struct scd4x_config {
	struct i2c_dt_spec bus;
	enum scd4x_model_t model;
	enum scd4x_mode_t mode;
};

struct scd4x_data {
	uint16_t temp_sample;
	uint16_t humi_sample;
	uint16_t co2_sample;
};

struct cmds_t {
	uint16_t cmd;
	uint16_t cmd_duration_ms;
};

#define SCD4X_CMD_REINIT                         0
#define SCD4X_CMD_START_PERIODIC_MEASUREMENT     1
#define SCD4X_CMD_STOP_PERIODIC_MEASUREMENT      2
#define SCD4X_CMD_READ_MEASUREMENT               3
#define SCD4X_CMD_SET_TEMPERATURE_OFFSET         4
#define SCD4X_CMD_GET_TEMPERATURE_OFFSET         5
#define SCD4X_CMD_SET_SENSOR_ALTITUDE            6
#define SCD4X_CMD_GET_SENSOR_ALTITUDE            7
#define SCD4X_CMD_SET_AMBIENT_PRESSURE           8
#define SCD4X_CMD_GET_AMBIENT_PRESSURE           9
#define SCD4X_CMD_FORCED_RECALIB                 10
#define SCD4X_CMD_SET_AUTOMATIC_CALIB_ENABLE     11
#define SCD4X_CMD_GET_AUTOMATIC_CALIB_ENABLE     12
#define SCD4X_CMD_LOW_POWER_PERIODIC_MEASUREMENT 13
#define SCD4X_CMD_GET_DATA_READY_STATUS          14
#define SCD4X_CMD_PERSIST_SETTINGS               15
#define SCD4X_CMD_SELF_TEST                      16
#define SCD4X_CMD_FACTORY_RESET                  17
#define SCD4X_CMD_MEASURE_SINGLE_SHOT            18
#define SCD4X_CMD_MEASURE_SINGLE_SHOT_RHT        19
#define SCD4X_CMD_POWER_DOWN                     20
#define SCD4X_CMD_WAKE_UP                        21
#define SCD4X_CMD_SET_SELF_CALIB_INITIAL_PERIOD  22
#define SCD4X_CMD_GET_SELF_CALIB_INITIAL_PERIOD  23
#define SCD4X_CMD_SET_SELF_CALIB_STANDARD_PERIOD 24
#define SCD4X_CMD_GET_SELF_CALIB_STANDARD_PERIOD 25

#define SCD4X_AMBIENT_PRESSURE_IDX_MAX   1200

#define SCD4X_MAX_TEMP 175
#define SCD4X_MIN_TEMP -45

#define SCD4X_CRC_POLY 0x31
#define SCD4X_CRC_INIT 0xFF

const struct cmds_t scd4x_cmds_MULTI[] = {
	{0x3646, 30},   {0x21B1, 0},  {0x3F86, 500}, {0xEC05, 1},   {0x241D, 1},     {0x2318, 1},
	{0x2427, 1},    {0x2322, 1},  {0xE000, 1},   {0xE000, 1},   {0x362F, 400},   {0x2416, 1},
	{0x2313, 1},    {0x21AC, 0},  {0xE4B8, 1},   {0x3615, 800}, {0x3639, 10000}, {0x3632, 1200},
	{0x219D, 5000}, {0x2196, 50}, {0x36E0, 1},   {0x36F6, 30},  {0x2445, 1},     {0x2340, 1},
	{0x244E, 1},    {0x234B, 1},
};

static uint8_t scd4x_calc_crc(uint16_t value)
{
	uint8_t buf[2];

	sys_put_be16(value, buf);

	return crc8(buf, 2, SCD4X_CRC_POLY, SCD4X_CRC_INIT, false);
}

static int scd4x_write_command(const struct device *dev, uint8_t cmd)
{
	const struct scd4x_config *cfg = dev->config;
	uint8_t tx_buf[2];
	int ret;

	sys_put_be16(scd4x_cmds_MULTI[cmd].cmd, tx_buf);

	ret = i2c_write_dt(&cfg->bus, tx_buf, sizeof(tx_buf));

	if (scd4x_cmds_MULTI[cmd].cmd_duration_ms) {
		k_msleep(scd4x_cmds_MULTI[cmd].cmd_duration_ms);
	}

	return ret;
}

static int scd4x_write_reg(const struct device *dev, uint8_t cmd, uint16_t *data, uint8_t data_size)
{
	const struct scd4x_config *cfg = dev->config;
	int ret;
	uint8_t tx_buf[((data_size * 3) + 2)];

	sys_put_be16(scd4x_cmds_MULTI[cmd].cmd, tx_buf);

	uint8_t tx_buf_pos = 2;

	for (uint8_t i = 0; i < data_size; i++) {
		sys_put_be16(data[i], &tx_buf[tx_buf_pos]);
		tx_buf_pos += 2;
		tx_buf[tx_buf_pos++] = scd4x_calc_crc(data[i]);
	}

	ret = i2c_write_dt(&cfg->bus, tx_buf, sizeof(tx_buf));
	if (ret < 0) {
		 //printk("Failed to write i2c data.");
		return ret;
	}

	if (scd4x_cmds_MULTI[cmd].cmd_duration_ms) {
		k_msleep(scd4x_cmds_MULTI[cmd].cmd_duration_ms);
	}
	return 0;
}

static int scd4x_read_reg(const struct device *dev, uint8_t *rx_buf, uint8_t rx_buf_size)
{
	const struct scd4x_config *cfg = dev->config;
	int ret;

	ret = i2c_read_dt(&cfg->bus, rx_buf, rx_buf_size);
	if (ret < 0) {
		// printk("Failed to read i2c data.");
		return ret;
	}

	for (uint8_t i = 0; i < (rx_buf_size / 3); i++) {
		ret = scd4x_calc_crc(sys_get_be16(&rx_buf[i * 3]));
		if (ret != rx_buf[(i * 3) + 2]) {
			// printk("Invalid CRC.");
			return -EIO;
		}
	}

	return 0;
}

int wakeup_scd41(const struct device *dev)
{
	/*send wake up command twice because of an expected nack return in power down mode*/
	scd4x_write_command(dev, SCD4X_CMD_WAKE_UP);
	int ret = scd4x_write_command(dev, SCD4X_CMD_WAKE_UP);
	if (ret < 0) {
		 //printk("Failed write wake_up command.");
		return ret;
	}

	return 0;
}

int power_down_scd41(const struct device *dev) 
{
	int ret = scd4x_write_command(dev, SCD4X_CMD_POWER_DOWN);
	if (ret < 0) {
		 //printk("Failed to write power_down command.");
		return ret;
	}

	return 0;
}

int _set_ambient_pressure_scd41(const struct device *dev, const struct sensor_value *val)
{
	int ret;

	uint16_t ambient_pressure = val->val1;

	ret = scd4x_write_reg(dev, SCD4X_CMD_SET_AMBIENT_PRESSURE, &ambient_pressure, 1);
	if (ret < 0) {
		 //printk("Failed to write set_ambient_pressure register.");
		return ret;
	}

	return 0;
}

int scd4x_forced_recalibration(const struct device *dev, uint16_t target_concentration,
			       uint16_t *frc_correction)
{
	uint8_t rx_buf[3];
	int ret;

	ret = scd4x_write_reg(dev, SCD4X_CMD_FORCED_RECALIB, &target_concentration, 1);
	if (ret < 0) {
		// printk("Failed to write perform_forced_recalibration register.");
		return ret;
	}

	ret = scd4x_read_reg(dev, rx_buf, sizeof(rx_buf));
	if (ret < 0) {
		// printk("Failed to read perform_forced_recalibration register.");
		return ret;
	}

	*frc_correction = sys_get_be16(rx_buf);

	/*from datasheet*/
	if (*frc_correction == 0xFFFF) {
		// printk("FRC failed. Returned 0xFFFF.");
		return -EIO;
	}

	*frc_correction -= 0x8000;

	return 0;
}

int set_ambient_pressure_scd41(const struct device *dev, const struct sensor_value *val)
{
	if (val->val1 > SCD4X_AMBIENT_PRESSURE_IDX_MAX || val->val1 < 700) {
		return -EINVAL;
	}
	int ret = _set_ambient_pressure_scd41(dev, val);
	if (ret < 0) {
		 //printk("Failed to set ambient pressure.");
		return ret;
	}
	
	return 0;
}

static int scd4x_channel_get(const struct device *dev, enum sensor_channel chan,
			     struct sensor_value *val)
{
	const struct scd4x_data *data = dev->data;
	int64_t tmp_val;

	switch ((enum sensor_channel)chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		/*Calculation from Datasheet*/
		tmp_val = data->temp_sample * SCD4X_MAX_TEMP;
		val->val1 = (int32_t)(tmp_val / 0xFFFF) + SCD4X_MIN_TEMP;
		val->val2 = ((tmp_val % 0xFFFF) * 1000000) / 0xFFFF;
		break;
	case SENSOR_CHAN_HUMIDITY:
		/*Calculation from Datasheet*/
		tmp_val = data->humi_sample * 100;
		val->val1 = (int32_t)(tmp_val / 0xFFFF);
		val->val2 = ((tmp_val % 0xFFFF) * 1000000) / 0xFFFF;
		break;
	case SENSOR_CHAN_CO2:
		val->val1 = data->co2_sample;
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

int get_co2_data_scd41(
	const struct device *dev,
	const struct sensor_value * pressure,
	const struct sensor_value * co2_measurement
) {
	int ret;

	ret = wakeup_scd41(dev);
	if (ret < 0) {
		return ret;
	}

	ret = set_ambient_pressure_scd41(dev, pressure);
	if (ret < 0) {
		return ret;
	}

	ret = scd4x_write_command(dev, SCD4X_CMD_MEASURE_SINGLE_SHOT);
	if (ret < 0) {
		return ret;
	}

	ret = scd4x_channel_get(dev, SENSOR_CHAN_CO2, co2_measurement);
	if (ret < 0) {
		return ret;
	}

	ret = power_down_scd41(dev);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int round_to_integer(double number) {
	int result = (int)(number + (number >= 0 ? 0.5 : -0.5));
	return result;
}

int prepare_measuring(void)
{
	if (!device_is_ready(scd_sensor))
	{
 		//printk("SCD41 %s is not ready.\n", scd_sensor->name);
		return -1;
	}


	struct sensor_value auto_calibration = {
		.val1 = 0,
		.val2 = 0,
	};

	sensor_attr_get(scd_sensor, SENSOR_CHAN_ALL, SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE, &auto_calibration);

	if (auto_calibration.val1 != 0) {
		auto_calibration.val1 = 0;
		sensor_attr_set(scd_sensor, SENSOR_CHAN_ALL, SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE, &auto_calibration);
	}

	if (!device_is_ready(bme_sensor))
	{
 		//printk("BME280 %s is not ready.\n", bme_sensor->name);
		return -1;
	}

	return 0;
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
 		//printk("failed sample fetch from %s\n", bme_sensor->name);
		return ret;
	}

	sensor_channel_get(bme_sensor, SENSOR_CHAN_AMBIENT_TEMP, temperature_measurement);
	sensor_channel_get(bme_sensor, SENSOR_CHAN_HUMIDITY, humidity_measurement);
	sensor_channel_get(bme_sensor, SENSOR_CHAN_PRESS, pressure_measurement);

 	//printk("bme temp %d %d\n", temperature_measurement->val1, temperature_measurement->val2);
 	//printk("bme hum %d %d\n", humidity_measurement->val1, humidity_measurement->val2);
 	//printk("bme press %d %d\n", pressure_measurement->val1, pressure_measurement->val2);

	// pressure in hecto pascals for correct CO2 measurement
	int pressure_correction = pressure_measurement->val1 * 10 + round_to_integer(pressure_measurement->val2 / 100000.0);

	struct sensor_value correction = {
		.val1 = pressure_correction,
		.val2 = 0,
	};


	ret = wakeup_scd41(scd_sensor);
	if (ret < 0) {
		return ret;
	}

	sensor_attr_set(scd_sensor, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &correction);

	ret = sensor_sample_fetch(scd_sensor);
	if (ret < 0)
	{
		//printk("failed sample fetch from %s\n", scd_sensor->name);
		return ret;
	}

	sensor_channel_get(scd_sensor, SENSOR_CHAN_CO2, co2_measurement);

	//printk("scd CO2 %d %d\n", co2_measurement->val1, co2_measurement->val2);
	return 0;
}

int recalibrate() {
	int ret = 0;
	struct bt_le_ext_adv* advertisement = NULL;

	ret = prepare_measuring();

	if (ret < 0) {
		return ret;
	}

	ret = prepare_bluetooth_advertising(&advertisement);
		
	if (ret < 0) {
		return ret;
	}

	struct sensor_value co2_measurement;
	struct sensor_value temperature_measurement;
	struct sensor_value pressure_measurement;
	struct sensor_value humidity_measurement;

	int64_t ready_to_calibration_time = k_uptime_get() + K_MINUTES(7);

	while (k_uptime_get() < ready_to_calibration_time)
	{
		ret = perform_measurement(
			&advertisement, 
			&co2_measurement,
			&temperature_measurement,
			&pressure_measurement,
			&humidity_measurement
		);

		if (ret < 0) {
			return ret;
		}
	}
	
	uint16_t frc_correction = 0;
	ret = wakeup_scd41(scd_sensor);
	
	if (ret < 0) {
		return ret;
	}

	ret = scd4x_forced_recalibration(scd_sensor, 420, &frc_correction);
	
	if (ret < 0) {
		return ret;
	}
	//printk("Correction applied: %d ppm", frc_correction);

	return ret;
}

int perform_measurement(
	struct bt_le_ext_adv** advertisement,
	struct sensor_value* co2_measurement,
	struct sensor_value* temperature_measurement,
	struct sensor_value* pressure_measurement,
	struct sensor_value* humidity_measurement
) {
	int ret = 0;
	do {
		ret = get_measurement_data(
			co2_measurement,
			temperature_measurement,
			pressure_measurement,
			humidity_measurement
		);
		if (ret < 0) {
			return ret;
		}
	} while (co2_measurement->val1 <= 0);

	int16_t temp_bt_home = (temperature_measurement->val1) * 100 + round_to_integer(temperature_measurement->val2 / 10000.0);
	int16_t humidity_bt_home = (humidity_measurement->val1) * 100 + round_to_integer(humidity_measurement->val2 / 10000.0);
	int32_t pressure_bt_home = pressure_measurement->val1 * 1000 + round_to_integer(pressure_measurement->val2 / 1000.0);
	int32_t co2_bt_home = co2_measurement->val1;
	float battery_voltage = read_battery_voltage();
	uint8_t battery_charge = ((battery_voltage - 3.4) * 100) / (4.1 - 3.4);

	if (*advertisement != NULL) {
		ret = update_service_data(
			advertisement,
			temp_bt_home,
			humidity_bt_home,
			pressure_bt_home,
			co2_bt_home,
			battery_charge
		);

		if (ret < 0) {
			return ret;
		}

		ret = start_advertising(advertisement);

		if (ret < 0) {
			return ret;
		}
	
		k_sleep(K_MSEC(3000));
		ret = stop_advertising(advertisement);

		if (ret < 0) {
			return ret;
		}
	} else {
		// to have the same power profile for SCD41 during calibration
		k_sleep(K_MSEC(3000));
	}

    k_sleep(K_MSEC(60000));

	return ret;
}

int main(void)
{
	special_boot_config config = {
        .count_for_special_boot = 3,
        .delay = 3000,
        .callback = recalibrate
    };

    int ret = init_special_boot(&config);
 	//printk("Starting SCD4x sensor app with nRF Connect SDK\n");

	struct bt_le_ext_adv *advertisement = NULL;

	struct sensor_value co2_measurement;
	struct sensor_value temperature_measurement;
	struct sensor_value pressure_measurement;
	struct sensor_value humidity_measurement;

	ret = prepare_measuring();
	if (ret < 0) {
		// printk("prepare_measuring failed\n");
		return ret;
	}

	ret = prepare_bluetooth_advertising(&advertisement);
	if (ret < 0) {
		// printk("prepare_bluetooth_advertising failed\n");
		return ret;
	}

	while (1)
	{
		ret = perform_measurement(
			&advertisement, 
			&co2_measurement,
			&temperature_measurement,
			&pressure_measurement,
			&humidity_measurement
		);
		if (ret < 0) {
			// printk("perform_measurement failed\n");
			return ret;
		}
	}

	return ret;
}
