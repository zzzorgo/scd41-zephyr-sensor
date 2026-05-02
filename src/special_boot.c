#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/sys/printk.h>

#define COUNTER_ID 1

typedef struct {
    uint32_t count_for_special_boot;
    uint32_t delay;
    int (*callback)(int counter_id);
} special_boot_config;

static struct nvs_fs fs;

int init_special_boot(special_boot_config *config) {
    int rc = 0;
    uint32_t counter = 0;
    uint32_t null_counter = 0;

    fs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
    fs.offset = FIXED_PARTITION_OFFSET(storage_partition);
    fs.sector_size = 0x2000;
    fs.sector_count = 4;

    rc = nvs_mount(&fs);
    if (rc) {
        printk("nvs_mount failed: %d\n", rc);
        return rc;
    }

    rc = nvs_read(&fs, COUNTER_ID, &counter, sizeof(counter));
    if (rc <= 0) {
        counter = 0;
    }

    /* reboot detected -> increment counter */
    counter++;

    nvs_write(&fs, COUNTER_ID, &counter, sizeof(counter));

    printk("boot counter: %u\n", counter);

    /* wait specified delay after reboot */
    k_sleep(K_MSEC(config->delay));

    nvs_write(&fs, COUNTER_ID, &null_counter, sizeof(counter));

    if (counter >= config->count_for_special_boot) {
        rc = config->callback(COUNTER_ID);
        return rc;
    } else {
        return rc;
    }
}
