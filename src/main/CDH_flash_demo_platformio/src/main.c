#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Hardware                                                           */
/* ------------------------------------------------------------------ */

static const struct gpio_dt_spec led1 =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

#define TEST_OFFSET     ((off_t)0x00100000)
#define ERASE_SIZE      4096u
#define MAX_MSG_LEN     (ERASE_SIZE - sizeof(uint32_t))

/* ------------------------------------------------------------------ */
/*  NVM init                                                           */
/* ------------------------------------------------------------------ */

int cdh_init_nv_memory(void)
{
    static int initialized;
    const struct device *nvm_dev =
        DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    uint64_t flash_size = 0;
    int rc;

    if (initialized) {
        return 0;
    }

    if (!device_is_ready(nvm_dev)) {
        printk("NVM init failed: device not ready.\n");
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
        bool jedec_ok = false;

        rc = flash_read_jedec_id(nvm_dev, jedec_id);
        if (rc != 0) {
            printk("NVM init failed: flash_read_jedec_id() error %d\n", rc);
            return -3;
        }

        if (jedec_id[0] == 0x9D && jedec_id[1] == 0x60 && jedec_id[2] == 0x18)
            jedec_ok = true;

        if (jedec_id[0] == 0x18 && jedec_id[1] == 0x9D && jedec_id[2] == 0x60) {
            jedec_ok = true;
            printk("NVM warning: JEDEC ID rotated: %02X %02X %02X\n",
                   jedec_id[0], jedec_id[1], jedec_id[2]);
        }

        if (!jedec_ok) {
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

/* ------------------------------------------------------------------ */
/*  Core flash helpers — take shell instance for output               */
/* ------------------------------------------------------------------ */

static int flash_write_message(const struct shell *sh, const char *msg)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    uint32_t msg_len = (uint32_t)strlen(msg);
    int rc;

    if (msg_len == 0 || msg_len > MAX_MSG_LEN) {
        shell_error(sh, "Message length %u out of range (1-%u)",
                    msg_len, (unsigned)MAX_MSG_LEN);
        return -EINVAL;
    }

    shell_print(sh, "Erasing sector at 0x%08X...", (unsigned)TEST_OFFSET);
    rc = flash_erase(dev, TEST_OFFSET, ERASE_SIZE);
    if (rc != 0) {
        shell_error(sh, "Erase failed (%d)", rc);
        return rc;
    }
    k_msleep(10);

    rc = flash_write(dev, TEST_OFFSET, &msg_len, sizeof(msg_len));
    if (rc != 0) {
        shell_error(sh, "Header write failed (%d)", rc);
        return rc;
    }

    rc = flash_write(dev, TEST_OFFSET + sizeof(msg_len), msg, msg_len);
    if (rc != 0) {
        shell_error(sh, "Body write failed (%d)", rc);
        return rc;
    }
    k_msleep(10);

    shell_print(sh, "Wrote %u bytes OK", msg_len);
    return 0;
}

static int flash_read_message(const struct shell *sh)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    static char buf[MAX_MSG_LEN + 1];
    uint32_t msg_len = 0;
    int rc;

    rc = flash_read(dev, TEST_OFFSET, &msg_len, sizeof(msg_len));
    if (rc != 0) {
        shell_error(sh, "Header read failed (%d)", rc);
        return rc;
    }

    if (msg_len == 0xFFFFFFFFu) {
        shell_warn(sh, "Sector is blank — use flash_write first");
        return -ENODATA;
    }

    if (msg_len == 0 || msg_len > MAX_MSG_LEN) {
        shell_error(sh, "Stored length %u is invalid — sector may be corrupt",
                    msg_len);
        return -EBADMSG;
    }

    rc = flash_read(dev, TEST_OFFSET + sizeof(msg_len), buf, msg_len);
    if (rc != 0) {
        shell_error(sh, "Body read failed (%d)", rc);
        return rc;
    }

    buf[msg_len] = '\0';
    shell_print(sh, "\"%s\" (%u bytes)", buf, msg_len);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Shell commands                                                     */
/* ------------------------------------------------------------------ */

static int cmd_flash_write(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: flash_write <message>");
        return -EINVAL;
    }

    static char msg_buf[MAX_MSG_LEN + 1];
    msg_buf[0] = '\0';

    for (size_t i = 1; i < argc; i++) {
        if (i > 1) {
            strncat(msg_buf, " ", sizeof(msg_buf) - strlen(msg_buf) - 1);
        }
        strncat(msg_buf, argv[i], sizeof(msg_buf) - strlen(msg_buf) - 1);
    }

    int rc = flash_write_message(sh, msg_buf);
    if (rc == 0) {
        shell_print(sh, "Verifying readback:");
        flash_read_message(sh);
    }

    return rc;
}

static int cmd_flash_read(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    return flash_read_message(sh);
}

SHELL_CMD_ARG_REGISTER(flash_write, NULL,
    "Write a message to NOR flash.\nUsage: flash_write <message>",
    cmd_flash_write, 2, 32);

SHELL_CMD_REGISTER(flash_read, NULL,
    "Read the message stored in NOR flash.",
    cmd_flash_read);

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    if (device_is_ready(led1.port)) {
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    }

    if (usb_enable(NULL) == 0) {
        k_msleep(1000);
        printk("\n--- CDH FLASH SHELL READY ---\n");
        printk("Commands: flash_write <msg>  |  flash_read\n\n");
    }

    if (cdh_init_nv_memory() != 0) {
        printk("ERROR: NVM init failed\n");
    }

    while (1) {
        if (device_is_ready(led1.port)) {
            gpio_pin_toggle_dt(&led1);
        }
        k_msleep(1000);
    }

    return 0;
}