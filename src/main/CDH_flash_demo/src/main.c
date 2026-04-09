#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

#define FLASH_TEST_OFFSET 0x00100000
#define FLASH_TEST_ERASE_SIZE 4096

static int demo_init_led(void)
{
    if (!device_is_ready(led1.port)) {
        printk("FLASH DEMO: LED device not ready\n");
        return -1;
    }

    return gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
}

static int demo_init_usb_console(void)
{
    if (!device_is_ready(console_dev)) {
        return -1;
    }

    return usb_enable(NULL);
}

static int demo_init_nv_memory(void)
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

        printk("NVM JEDEC ID: %02X %02X %02X\n",
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

static int demo_flash_test_deadbeef(void)
{
    const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    const uint32_t tx_value = 0xDEADBEEF;
    uint32_t rx_value = 0;
    int rc;

    if (!device_is_ready(flash_dev)) {
        printk("FLASH TEST: device not ready\n");
        return -1;
    }

    rc = flash_erase(flash_dev, FLASH_TEST_OFFSET, FLASH_TEST_ERASE_SIZE);
    if (rc != 0) {
        printk("FLASH TEST: erase failed (%d)\n", rc);
        return -2;
    }

    rc = flash_write(flash_dev, FLASH_TEST_OFFSET, &tx_value, sizeof(tx_value));
    if (rc != 0) {
        printk("FLASH TEST: write failed (%d)\n", rc);
        return -3;
    }

    rc = flash_read(flash_dev, FLASH_TEST_OFFSET, &rx_value, sizeof(rx_value));
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
    if (demo_init_led() != 0) {
        return 0;
    }

    if (demo_init_usb_console() == 0) {
        k_msleep(1200);
        printk("\n--- CDH FLASH DEMO START ---\n");
        printk("USB console live\n");
    }

    if (demo_init_nv_memory() == 0) {
        demo_flash_test_deadbeef();
    } else {
        printk("FLASH DEMO: init failed\n");
    }

    while (1) {
        gpio_pin_toggle_dt(&led1);
        k_msleep(1000);
    }

    return 0;
}
