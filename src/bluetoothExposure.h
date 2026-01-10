#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci.h>

int start_advertising(struct bt_le_ext_adv **full_advertisement);

int update_service_data(
	struct bt_le_ext_adv **full_advertisement,
	int16_t temp_bt_home,
	int16_t humidity_bt_home,
	int32_t pressure_bt_home,
	int32_t co2_bt_home,
	uint8_t battery_charge
);
