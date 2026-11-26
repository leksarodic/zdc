#include "bt_uart.h"

#include <string.h>

uint8_t bt_uart_checksum(const uint8_t *body, size_t body_len) {
    unsigned sum = 0;
    for (size_t i = 0; i < body_len; i++) sum += body[i];
    return (uint8_t)(0x100u - (sum & 0xFFu));
}

size_t bt_uart_encode(uint8_t *out, size_t cap, uint8_t cmd, const uint8_t *data, uint8_t data_len) {
    if (data_len > BT_UART_MAX_DATA) return 0;
    size_t total = (size_t)data_len + 5u;
    if (cap < total) return 0;

    out[0] = BT_UART_SYNC0;
    out[1] = BT_UART_SYNC1;
    out[2] = data_len;
    out[3] = cmd;
    if (data_len && data) memcpy(out + 4, data, data_len);
    out[4 + data_len] = bt_uart_checksum(out + 2, (size_t)data_len + 2u);
    return total;
}

bool bt_uart_decode(const uint8_t *buf, size_t len, uint8_t *cmd,
                    uint8_t *data, uint8_t *data_len, size_t *consumed) {
    if (!buf || len < 4) return false;

    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] != BT_UART_SYNC0 || buf[i + 1] != BT_UART_SYNC1) continue;
        uint8_t n = buf[i + 2];
        if (n > BT_UART_MAX_DATA) continue;
        size_t total = (size_t)n + 5u;
        if (i + total > len) continue;

        uint8_t got = buf[i + total - 1];
        uint8_t want = bt_uart_checksum(buf + i + 2, (size_t)n + 2u);
        if (got != want) continue;

        if (cmd) *cmd = buf[i + 3];
        if (data && n) memcpy(data, buf + i + 4, n);
        if (data_len) *data_len = n;
        if (consumed) *consumed = i + total;
        return true;
    }
    return false;
}
