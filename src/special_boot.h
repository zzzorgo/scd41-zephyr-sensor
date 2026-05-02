#ifndef SPECIAL_BOOT_H
#define SPECIAL_BOOT_H

typedef struct {
    uint32_t count_for_special_boot;
    uint32_t delay;
    int (*callback)(int counter_id);
} special_boot_config;

int init_special_boot(special_boot_config *config);

#endif /* SPECIAL_BOOT_H */
