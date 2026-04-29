/*
 * CDH Image Demo — main.c
 *
 * Receives TAB/OpenLST framed image data over UART1 (hardware UART,
 * same pin as COM board interface), stores it to the IS25LP128 QSPI
 * flash payload region at 0x00115000, and can dump it back over UART1
 * for PC-side reconstruction.
 *
 * USB console (UART0) is used for status/debug printk only.
 * UART1 is the TAB data channel.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>

#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Hardware                                                           */
/* ------------------------------------------------------------------ */

static const struct gpio_dt_spec led1 =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct device *uart1;
static const struct device *flash_dev;

/* ------------------------------------------------------------------ */
/*  TAB framing constants                                              */
/* ------------------------------------------------------------------ */

#define TAB_SYNC0           0x22
#define TAB_SYNC1           0x69
#define TAB_CMD_IMG_START   0x51
#define TAB_CMD_IMG_CHUNK   0x50
#define TAB_CMD_IMG_END     0x52
#define TAB_CMD_IMG_REQ     0x53    /* PC requests readback            */
#define TAB_CMD_IMG_ACK     0x54    /* firmware ACK per chunk          */
#define TAB_MAX_MSG         320     /* max bytes in one TAB message    */
#define CHUNK_SIZE          64      /* image bytes per chunk           */

/* ------------------------------------------------------------------ */
/*  Flash layout                                                       */
/* ------------------------------------------------------------------ */

#define SECTOR_SIZE         4096u
#define PAYLOAD_BASE        0x00115000u
#define PAYLOAD_HDR_OFFSET  PAYLOAD_BASE
#define PAYLOAD_DATA_OFFSET (PAYLOAD_BASE + SECTOR_SIZE)
#define PAYLOAD_MAX_DATA    (63u * SECTOR_SIZE)  /* 63 sectors = 252KB */

#define HDR_MAGIC           0xDA7ADA7Au

typedef struct {
    uint32_t magic;
    uint32_t image_id;
    uint32_t total_size;
    uint32_t stored_size;
    uint32_t timestamp_ms;
    uint16_t chunks_total;
    uint16_t chunks_stored;
    uint8_t  format;        /* 0=raw 1=jpeg 2=png                     */
    uint8_t  complete;
    uint8_t  _pad[2];
    uint32_t crc32;
} img_hdr_t;

/* ------------------------------------------------------------------ */
/*  UART RX ring buffer                                                */
/* ------------------------------------------------------------------ */

#define RX_BUF_SIZE 512

static uint8_t  rx_buf[RX_BUF_SIZE];
static uint32_t rx_head = 0;
static uint32_t rx_tail = 0;

static void uart_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    if (!uart_irq_update(dev)) return;
    while (uart_irq_rx_ready(dev)) {
        uint8_t c;
        if (uart_fifo_read(dev, &c, 1) == 1) {
            uint32_t next = (rx_head + 1) % RX_BUF_SIZE;
            if (next != rx_tail) {
                rx_buf[rx_head] = c;
                rx_head = next;
            }
        }
    }
}

static bool uart_read_byte(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t deadline = k_uptime_get_32() + timeout_ms;
    while (k_uptime_get_32() < deadline) {
        if (rx_head != rx_tail) {
            *out = rx_buf[rx_tail];
            rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
            return true;
        }
        k_msleep(1);
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  UART TX helpers                                                    */
/* ------------------------------------------------------------------ */

static void uart_send(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart1, buf[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  CRC-16/CCITT                                                       */
/* ------------------------------------------------------------------ */

static uint16_t tab_crc16(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

/* ------------------------------------------------------------------ */
/*  Header helpers                                                     */
/* ------------------------------------------------------------------ */

static uint32_t hdr_crc(const img_hdr_t *h)
{
    uint32_t c = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)h;
    for (size_t i = 0; i < offsetof(img_hdr_t, crc32); i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : c >> 1;
    }
    return c ^ 0xFFFFFFFF;
}

static int read_hdr(img_hdr_t *hdr)
{
    int rc = flash_read(flash_dev, PAYLOAD_HDR_OFFSET, hdr, sizeof(*hdr));
    if (rc != 0) return rc;
    if (hdr->magic == 0xFFFFFFFFu) return 1;   /* blank              */
    if (hdr->magic != HDR_MAGIC)   return -EBADMSG;
    if (hdr->crc32 != hdr_crc(hdr)) return -EILSEQ;
    return 0;
}

static int write_hdr(img_hdr_t *hdr)
{
    hdr->magic = HDR_MAGIC;
    hdr->crc32 = hdr_crc(hdr);
    int rc = flash_erase(flash_dev, PAYLOAD_HDR_OFFSET, SECTOR_SIZE);
    if (rc != 0) return rc;
    k_msleep(10);
    return flash_write(flash_dev, PAYLOAD_HDR_OFFSET, hdr, sizeof(*hdr));
}

/* ------------------------------------------------------------------ */
/*  TAB message receive — blocks until full message or timeout        */
/* ------------------------------------------------------------------ */

static int tab_receive(uint8_t *out, size_t out_max, uint32_t timeout_ms)
{
    uint8_t b;

    /* Hunt for sync bytes */
    while (true) {
        if (!uart_read_byte(&b, timeout_ms)) return -ETIMEDOUT;
        if (b != TAB_SYNC0) continue;
        if (!uart_read_byte(&b, 100))        return -ETIMEDOUT;
        if (b == TAB_SYNC0) continue;        /* double sync — retry  */
        if (b != TAB_SYNC1) continue;
        break;
    }

    out[0] = TAB_SYNC0;
    out[1] = TAB_SYNC1;

    /* Read length byte */
    if (!uart_read_byte(&b, 100)) return -ETIMEDOUT;
    out[2] = b;
    uint8_t inner_len = b;

    /* Total message = sync(2) + len(1) + inner_len + crc(2) */
    size_t total = 3u + inner_len + 2u;
    if (total > out_max) return -ENOMEM;

    /* Read remainder */
    for (size_t i = 3; i < total; i++) {
        if (!uart_read_byte(&out[i], 100)) return -ETIMEDOUT;
    }

    /* Verify CRC */
    uint16_t rx_crc  = (uint16_t)out[total - 2] |
                       ((uint16_t)out[total - 1] << 8);
    uint16_t calc_crc = tab_crc16(out + 2, total - 4);
    if (rx_crc != calc_crc) {
        printk("TAB RX: CRC fail rx=0x%04X calc=0x%04X\n",
               rx_crc, calc_crc);
        return -EBADMSG;
    }

    return (int)total;
}

/* ------------------------------------------------------------------ */
/*  Build and send a TAB ACK message                                  */
/* ------------------------------------------------------------------ */

static void tab_send_ack(uint16_t chunk_index, uint8_t status)
{
    uint8_t msg[16];
    /* inner: dst(2)+src(2)+seq(2)+cmd(1)+chunk_index(2)+status(1) = 10 */
    msg[0] = TAB_SYNC0;
    msg[1] = TAB_SYNC1;
    msg[2] = 10;                        /* len                        */
    msg[3] = 0x00; msg[4] = 0x02;      /* dst = payload (0x0002)     */
    msg[5] = 0x00; msg[6] = 0x01;      /* src = CDH    (0x0001)      */
    msg[7] = (uint8_t)(chunk_index);
    msg[8] = (uint8_t)(chunk_index >> 8);
    msg[9] = TAB_CMD_IMG_ACK;
    msg[10] = (uint8_t)(chunk_index);
    msg[11] = (uint8_t)(chunk_index >> 8);
    msg[12] = status;
    uint16_t crc = tab_crc16(msg + 2, 11);
    msg[13] = (uint8_t)(crc);
    msg[14] = (uint8_t)(crc >> 8);
    uart_send(msg, 15);
}

/* ------------------------------------------------------------------ */
/*  Process one received TAB message                                  */
/* ------------------------------------------------------------------ */

static int process_tab(const uint8_t *buf, size_t len)
{
    /* cmd byte is at offset 9 (after sync+len+dst+src+seq) */
    if (len < 12) return -EINVAL;
    uint8_t cmd = buf[9];

    /* data starts after cmd byte, ends before 2-byte CRC */
    const uint8_t *data = buf + 10;
    size_t data_len = len - 12;  /* total - sync(2) - len(1) - hdr(6) - cmd(1) - crc(2) */

    if (cmd == TAB_CMD_IMG_START) {
        /* data: image_id(4) + total_size(4) + chunk_size(2) + format(1) */
        if (data_len < 11) return -EINVAL;
        uint32_t image_id   = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                              ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        uint32_t total_size = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                              ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        uint16_t chunk_size = (uint16_t)data[8] | ((uint16_t)data[9] << 8);
        uint8_t  format     = data[10];

        /* Erase header + first data sector */
        flash_erase(flash_dev, PAYLOAD_HDR_OFFSET, SECTOR_SIZE);
        k_msleep(10);
        flash_erase(flash_dev, PAYLOAD_DATA_OFFSET, SECTOR_SIZE);
        k_msleep(10);

        img_hdr_t hdr = {
            .image_id      = image_id,
            .total_size    = total_size,
            .stored_size   = 0,
            .timestamp_ms  = (uint32_t)k_uptime_get(),
            .chunks_total  = (uint16_t)((total_size + chunk_size - 1) / chunk_size),
            .chunks_stored = 0,
            .format        = format,
            .complete      = 0,
        };
        int rc = write_hdr(&hdr);
        printk("IMG START id=0x%08X size=%u fmt=%u rc=%d\n",
               image_id, total_size, format, rc);
        tab_send_ack(0xFFFF, rc == 0 ? 0 : 1);
        return rc;
    }

    if (cmd == TAB_CMD_IMG_CHUNK) {
        /* data: image_id(4) + chunk_index(2) + chunk_data */
        if (data_len < 7) return -EINVAL;
        uint16_t chunk_idx = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
        const uint8_t *chunk_data = data + 6;
        size_t chunk_data_len = data_len - 6;

        img_hdr_t hdr;
        if (read_hdr(&hdr) != 0) { tab_send_ack(chunk_idx, 2); return -ENODATA; }

        off_t write_off = PAYLOAD_DATA_OFFSET + hdr.stored_size;
        if (hdr.stored_size + chunk_data_len > PAYLOAD_MAX_DATA) {
            tab_send_ack(chunk_idx, 3);
            return -ENOSPC;
        }

        /* Erase sector if on boundary */
        if ((write_off % SECTOR_SIZE) == 0) {
            flash_erase(flash_dev, write_off, SECTOR_SIZE);
            k_msleep(5);
        }

        int rc = flash_write(flash_dev, write_off, chunk_data, chunk_data_len);
        if (rc != 0) { tab_send_ack(chunk_idx, 4); return rc; }

        hdr.stored_size   += chunk_data_len;
        hdr.chunks_stored += 1;
        write_hdr(&hdr);

        printk("CHUNK[%u] %u bytes @ 0x%08X (%u/%u)\n",
               chunk_idx, (unsigned)chunk_data_len,
               (unsigned)write_off, hdr.stored_size, hdr.total_size);
        tab_send_ack(chunk_idx, 0);
        return 0;
    }

    if (cmd == TAB_CMD_IMG_END) {
        img_hdr_t hdr;
        if (read_hdr(&hdr) != 0) return -ENODATA;
        hdr.complete = 1;
        int rc = write_hdr(&hdr);
        printk("IMG END — %u/%u bytes stored, complete=%d\n",
               hdr.stored_size, hdr.total_size, rc == 0);
        tab_send_ack(0xFFFE, rc == 0 ? 0 : 1);
        return rc;
    }

    if (cmd == TAB_CMD_IMG_REQ) {
        /* PC is requesting readback — stream all stored bytes back    */
        img_hdr_t hdr;
        int rc = read_hdr(&hdr);
        if (rc == 1) {
            printk("IMG REQ: no image stored\n");
            tab_send_ack(0xFFFD, 5);
            return -ENODATA;
        }
        if (rc != 0) {
            tab_send_ack(0xFFFD, 6);
            return rc;
        }

        printk("IMG REQ: streaming %u bytes back to PC\n", hdr.stored_size);

        static uint8_t chunk_buf[CHUNK_SIZE];
        uint32_t sent = 0;
        uint16_t idx  = 0;

        while (sent < hdr.stored_size) {
            size_t to_read = hdr.stored_size - sent;
            if (to_read > CHUNK_SIZE) to_read = CHUNK_SIZE;

            flash_read(flash_dev, PAYLOAD_DATA_OFFSET + sent,
                       chunk_buf, to_read);

            /* Build and send TAB IMG_CHUNK back to PC */
            uint8_t out[TAB_MAX_MSG];
            uint8_t chunk_hdr[6];
            /* image_id(4) + chunk_idx(2) */
            chunk_hdr[0] = (uint8_t)(hdr.image_id);
            chunk_hdr[1] = (uint8_t)(hdr.image_id >> 8);
            chunk_hdr[2] = (uint8_t)(hdr.image_id >> 16);
            chunk_hdr[3] = (uint8_t)(hdr.image_id >> 24);
            chunk_hdr[4] = (uint8_t)(idx);
            chunk_hdr[5] = (uint8_t)(idx >> 8);

            uint8_t inner_len = 6 + 1 + 6 + (uint8_t)to_read;
            out[0] = TAB_SYNC0; out[1] = TAB_SYNC1;
            out[2] = inner_len;
            out[3] = 0x00; out[4] = 0x02;  /* dst = PC               */
            out[5] = 0x00; out[6] = 0x01;  /* src = CDH              */
            out[7] = (uint8_t)(idx); out[8] = (uint8_t)(idx >> 8);
            out[9] = TAB_CMD_IMG_CHUNK;
            memcpy(out + 10, chunk_hdr, 6);
            memcpy(out + 16, chunk_buf, to_read);
            size_t total = 3u + inner_len + 2u;
            uint16_t crc = tab_crc16(out + 2, total - 4);
            out[total - 2] = (uint8_t)(crc);
            out[total - 1] = (uint8_t)(crc >> 8);
            uart_send(out, total);

            sent += to_read;
            idx++;
            k_msleep(2);  /* small delay to not overwhelm PC buffer   */
        }

        /* Send IMG_END to signal done */
        uint8_t end_msg[14];
        end_msg[0] = TAB_SYNC0; end_msg[1] = TAB_SYNC1;
        end_msg[2] = 8;
        end_msg[3] = 0x00; end_msg[4] = 0x02;
        end_msg[5] = 0x00; end_msg[6] = 0x01;
        end_msg[7] = 0xFF; end_msg[8] = 0xFF;
        end_msg[9] = TAB_CMD_IMG_END;
        end_msg[10] = hdr.format;
        end_msg[11] = (uint8_t)(hdr.stored_size);
        end_msg[12] = (uint8_t)(hdr.stored_size >> 8);
        uint16_t crc = tab_crc16(end_msg + 2, 11);
        end_msg[12] = (uint8_t)(crc);
        end_msg[13] = (uint8_t)(crc >> 8);
        uart_send(end_msg, 14);

        printk("IMG REQ: readback complete, %u chunks sent\n", idx);
        return 0;
    }

    printk("TAB: unknown cmd 0x%02X\n", cmd);
    return -ENOTSUP;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    /* LED */
    if (device_is_ready(led1.port)) {
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    }

    /* USB console for debug printk */
    if (usb_enable(NULL) == 0) {
        k_msleep(1000);
    }

    printk("\n--- CDH IMAGE DEMO START ---\n");

    /* Flash */
    flash_dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    if (!device_is_ready(flash_dev)) {
        printk("FATAL: flash not ready\n");
        goto spin;
    }
    printk("Flash OK\n");

    /* UART1 — TAB data channel */
    uart1 = DEVICE_DT_GET(DT_NODELABEL(usart1));
    if (!device_is_ready(uart1)) {
        printk("FATAL: UART1 not ready\n");
        goto spin;
    }
    uart_irq_callback_set(uart1, uart_cb);
    uart_irq_rx_enable(uart1);
    printk("UART1 ready — waiting for TAB messages\n");

    /* Check if we already have a stored image from a previous run */
    img_hdr_t hdr;
    int rc = read_hdr(&hdr);
    if (rc == 0) {
        printk("Existing image: id=0x%08X size=%u complete=%s\n",
               hdr.image_id, hdr.stored_size,
               hdr.complete ? "YES" : "NO");
    } else {
        printk("No image stored — send IMG_START to begin\n");
    }

    /* Main loop — wait for TAB messages on UART1 */
    static uint8_t msg_buf[TAB_MAX_MSG];
    while (1) {
        int msg_len = tab_receive(msg_buf, sizeof(msg_buf), 5000);

        if (msg_len == -ETIMEDOUT) {
            /* heartbeat */
            if (device_is_ready(led1.port)) {
                gpio_pin_toggle_dt(&led1);
            }
            continue;
        }

        if (msg_len < 0) {
            printk("TAB RX error %d — discarding\n", msg_len);
            continue;
        }

        process_tab(msg_buf, (size_t)msg_len);
    }

spin:
    while (1) { k_msleep(1000); }
    return 0;
}
