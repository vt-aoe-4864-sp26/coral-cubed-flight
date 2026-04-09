#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static int cdh_init_nv_memory(void)
{
    static int initialized;
    const struct device *nvm_dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    uint64_t flash_size = 0;
    int rc;

    if (initialized) {
        return 0;
    }

    if (!device_is_ready(nvm_dev)) {
        printk("NVM init failed: is25lp128 device not ready.\n");
        return -1;
    }

    rc = flash_get_size(nvm_dev, &flash_size);
    if (rc != 0) {
        printk("NVM init failed: flash_get_size() error %d\n", rc);
        return -2;
    }

#if defined(CONFIG_FLASH_JESD216_API)
    {
        uint8_t jedec_id[3] = {0};

        rc = flash_read_jedec_id(nvm_dev, jedec_id);
        if (rc != 0) {
            printk("NVM init failed: flash_read_jedec_id() error %d\n", rc);
            return -3;
        }

        if ((jedec_id[0] != 0x9D) || (jedec_id[1] != 0x60) || (jedec_id[2] != 0x18)) {
            printk("NVM init failed: unexpected JEDEC ID %02X %02X %02X\n",
                   jedec_id[0], jedec_id[1], jedec_id[2]);
            return -4;
        }

        printk("NVM JEDEC ID OK: %02X %02X %02X\n",
               jedec_id[0], jedec_id[1], jedec_id[2]);
    }
#endif

    printk("NVM ready: %s, size=%llu bytes, write_block=%zu\n",
           nvm_dev->name,
           (unsigned long long)flash_size,
           flash_get_write_block_size(nvm_dev));

    initialized = 1;
    return 0;
}

static int cdh_flash_test_deadbeef(void)
{
    const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    const off_t test_offset = 0x00100000;
    const size_t erase_size = 4096;
    const uint32_t tx_value = 0xDEADBEEF;
    uint32_t rx_value = 0;
    int rc;

    if (!device_is_ready(flash_dev)) {
        printk("FLASH TEST: device not ready\n");
        return -1;
    }

    rc = flash_erase(flash_dev, test_offset, erase_size);
    if (rc != 0) {
        printk("FLASH TEST: erase failed (%d)\n", rc);
        return -2;
    }

    rc = flash_write(flash_dev, test_offset, &tx_value, sizeof(tx_value));
    if (rc != 0) {
        printk("FLASH TEST: write failed (%d)\n", rc);
        return -3;
    }

    rc = flash_read(flash_dev, test_offset, &rx_value, sizeof(rx_value));
    if (rc != 0) {
        printk("FLASH TEST: read failed (%d)\n", rc);
        return -4;
    }

    printk("FLASH TEST: read back = 0x%08X\n", rx_value);

    if (rx_value != tx_value) {
        printk("FLASH TEST: mismatch, expected 0x%08X\n", tx_value);
        return -5;
    }

    printk("FLASH TEST: PASS\n");
    return 0;
}

int main(void)
{
    if (device_is_ready(led1.port)) {
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    }

    if (usb_enable(NULL) == 0) {
        k_msleep(1000);
        printk("\n--- CDH FLASH DEMO START ---\n");
        printk("USB console live\n");
    }

    if (cdh_init_nv_memory() == 0) {
        cdh_flash_test_deadbeef();
    } else {
        printk("FLASH DEMO: init failed\n");
    }

    while (1) {
        if (device_is_ready(led1.port)) {
            gpio_pin_toggle_dt(&led1);
        }
        k_msleep(1000);
    }

    return 0;
}
