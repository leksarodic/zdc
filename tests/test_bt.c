#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bt_sim.h"
#include "bt_uart.h"

static void test_frame(void) {
    uint8_t buf[BT_UART_MAX_FRAME];
    uint8_t ctrl[] = {0x01};
    size_t n = bt_uart_encode(buf, sizeof(buf), 0x81, ctrl, 1);
    assert(n == 6);
    assert(buf[0] == 0x55 && buf[1] == 0xAA);
    assert(buf[2] == 1 && buf[3] == 0x81 && buf[4] == 0x01);
    assert(buf[5] == 0x7D);

    uint8_t cmd = 0, data[BT_UART_MAX_DATA], dlen = 0;
    size_t used = 0;
    assert(bt_uart_decode(buf, n, &cmd, data, &dlen, &used));
    assert(cmd == 0x81 && dlen == 1 && data[0] == 0x01 && used == n);

    uint8_t zeros[8] = {0};
    n = bt_uart_encode(buf, sizeof(buf), 0x80, zeros, 8);
    assert(n == 13);
    assert(buf[2] == 8 && buf[3] == 0x80);
    assert(buf[12] == 0x78); /* 55 AA 08 80 00*8 78 */
}

static void test_state(void) {
    bt_model_t m;
    bt_sim_init(&m);
    assert(m.state == BT_CONNECTED);

    bt_sim_primary(&m);
    assert(m.state == BT_PLAYING);
    bt_sim_next(&m);
    assert(m.state == BT_PLAYING);
    assert(m.track_index == 1);

    bt_sim_set_state(&m, BT_INCOMING);
    assert(m.music_interrupted);
    bt_sim_primary(&m);
    assert(m.state == BT_ACTIVE);
    assert(bt_sim_music_interrupted(&m));
    bt_sim_secondary(&m);
    assert(m.muted);
    bt_sim_primary(&m);
    assert(m.state == BT_PLAYING);
    assert(!m.music_interrupted);

    bt_sim_set_state(&m, BT_READY);
    bt_sim_primary(&m);
    assert(m.state == BT_PAIRING);
    bt_sim_tick(&m, 1);
    assert(m.state == BT_PAIRING);
    bt_sim_tick(&m, 1);
    assert(m.state == BT_CONNECTED);
}

int main(void) {
    test_frame();
    test_state();
    puts("bt tests: OK");
    return 0;
}
