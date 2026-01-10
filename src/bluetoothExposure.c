#include "bluetoothExposure.h"

#define SERVICE_DATA_LEN 18
#define SERVICE_UUID 0xfcd2

#define ADV_PARAM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_EXT_ADV, \
				  BT_GAP_ADV_SLOW_INT_MIN, \
				  BT_GAP_ADV_SLOW_INT_MAX, NULL)

enum bt_sample_adv_evt {
	BT_SAMPLE_EVT_CONNECTED,
	BT_SAMPLE_EVT_DISCONNECTED,
	BT_SAMPLE_EVT_MAX,
};

enum bt_sample_adv_st {
	BT_SAMPLE_ST_ADV,
	BT_SAMPLE_ST_CONNECTED,
};

static volatile enum bt_sample_adv_st app_st = BT_SAMPLE_ST_ADV;

static struct k_poll_signal poll_sig = K_POLL_SIGNAL_INITIALIZER(poll_sig);

static uint8_t service_data[SERVICE_DATA_LEN] = {
	BT_UUID_16_ENCODE(SERVICE_UUID),
	0x40,   /* BTHome Device Information */
	0x02,	/* Temperature */
	0xc4,	/* Low byte */
	0x00,   /* High byte */
	0x03,	/* Humidity */
	0xbf,	/* 50.55%  low byte*/
	0x13,   /* 50.55%  high byte*/
	0x04,   /* Preassure hPa */
	0x00,   /* byte 1 */
	0x00,   /* byte 2 */
	0x00,   /* byte 3 */
	0x12,   /* CO2 */
	0x00,   /* byte 1 */
	0x00,   /* byte 2 */
	0x01,   /* Battery */
	0x00    /* byte 1 */
};

// static struct bt_data ad[] = {
static struct bt_data advertisement_payload[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA(BT_DATA_SVC_DATA16, service_data, ARRAY_SIZE(service_data))
};

int _start_advertising(struct bt_le_ext_adv *full_advertisement)
{
	int err;

	printk("Starting Extended Advertising\n");
	err = bt_le_ext_adv_start(full_advertisement, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start extended advertising (err %d)\n", err);
	}

	return err;
}

int start_advertising(struct bt_le_ext_adv **full_advertisement)
{
	int err;

	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_CONN, NULL, full_advertisement);

	printk("full_advertisement %p\n", (void*) full_advertisement);

	if (err) {
		printk("Failed to create advertising set (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(*full_advertisement, advertisement_payload, ARRAY_SIZE(advertisement_payload), NULL, 0);

	if (err) {
		printk("Failed to set advertising data (err %d)\n", err);
		return err;
	}

	err = _start_advertising(*full_advertisement);

	if (err) {
		return err;
	}

	return 0;
}

int update_service_data(
	struct bt_le_ext_adv **full_advertisement,
	int16_t temp_bt_home,
	int16_t humidity_bt_home,
	int32_t pressure_bt_home,
	int32_t co2_bt_home,
	uint8_t battery_charge
)
{
	int err;

	service_data[4] = temp_bt_home & 0xFF;
	service_data[5] = (temp_bt_home >> 8) & 0xFF;

	service_data[7] = humidity_bt_home & 0xFF;
	service_data[8] = (humidity_bt_home >> 8) & 0xFF;

	service_data[10] = pressure_bt_home & 0xFF;
	service_data[11] = (pressure_bt_home >> 8) & 0xFF;
	service_data[12] = (pressure_bt_home >> 16) & 0xFF;

	service_data[14] = co2_bt_home & 0xFF;
	service_data[15] = (co2_bt_home >> 8) & 0xFF;

	service_data[17] = battery_charge;

	err = bt_le_ext_adv_set_data(*full_advertisement, advertisement_payload, ARRAY_SIZE(advertisement_payload), NULL, 0);

	if (err) {
		printk("Failed to update advertising data (err %d)\n", err);
		return err;
	}

	return 0;
}
