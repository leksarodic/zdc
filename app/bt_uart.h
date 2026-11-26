#ifndef STRADA_BT_UART_H
#define STRADA_BT_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Recovered 7010B MCU/BT UART frame:
 *
 *   55 AA LEN CMD DATA[LEN] CHK
 *   CHK = (0x100 - ((LEN + CMD + sum(DATA)) & 0xFF)) & 0xFF
 *
 * The stock driver is ramdisk.iso/uart_com.drv V5.5.
 * This is the link layer only; the semantic command map is board-specific.
 */

#define BT_UART_SYNC0 0x55u
#define BT_UART_SYNC1 0xAAu
#define BT_UART_MAX_DATA 64u
#define BT_UART_MAX_FRAME (BT_UART_MAX_DATA + 5u)

uint8_t bt_uart_checksum(const uint8_t *body, size_t body_len);

/* Returns the frame length, or 0 if cap is too small / data is too long. */
size_t bt_uart_encode(uint8_t *out, size_t cap, uint8_t cmd, const uint8_t *data, uint8_t data_len);

/* Scans buf for a valid frame. On success returns true and fills cmd/data/len.
 * *consumed is set to the number of bytes to skip (including any leading garbage). */
bool bt_uart_decode(const uint8_t *buf, size_t len, uint8_t *cmd,
                    uint8_t *data, uint8_t *data_len, size_t *consumed);

#endif
