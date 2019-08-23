#include <string.h>
#include "btstack.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "spi.h"

spi_slave_transaction_t transaction;
uint8_t * recv_buf;
uint8_t * send_buf;
uint8_t send_buf_pos = 0;
uint8_t recv_data_ready = 0;

// Called when master has completed a transaction
void spi_slave_post_trans_cb() {
    recv_data_ready = 1;
    gpio_set_level(SPI_EN, 0);
}

void spi_slave_init(uint64_t mosi_pin, uint64_t miso_pin, uint64_t sclk_pin, uint64_t cs_pin) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi_pin,
        .miso_io_num = miso_pin,
        .sclk_io_num = sclk_pin
    };

    spi_slave_interface_config_t slv_cfg = {
        .mode = 1,
        .spics_io_num = cs_pin,
        .queue_size = 8,
        .flags = 0,
        //.post_setup_cb = spi_slave_post_setup_cb,
        .post_trans_cb = spi_slave_post_trans_cb
    };

    spi_slave_initialize(VSPI_HOST, &bus_cfg, &slv_cfg, 1);
    memset(&transaction, 0, sizeof(transaction));

    recv_buf = heap_caps_malloc(128, MALLOC_CAP_DMA);
    send_buf = heap_caps_malloc(128, MALLOC_CAP_DMA);
}

uint8_t spi_slave_queue_data(uint8_t * data_buf, uint8_t data_len) {
    if (data_len + send_buf_pos > 128) data_len = 128 - send_buf_pos;
    memcpy(send_buf + send_buf_pos, data_buf, data_len);
    send_buf_pos += data_len;

    // Queue transaction after 128 bytes have been written
    if (send_buf_pos == 128) {
        send_buf_pos = 0;
        transaction.length = 1024;
        transaction.tx_buffer = send_buf;
        transaction.rx_buffer = recv_buf;
        if (spi_slave_queue_trans(VSPI_HOST, &transaction, 0) == ESP_OK) return 1;
    }
    return 0;
}

uint8_t * spi_slave_get_data(uint8_t * data_len) {
    if (recv_data_ready) {
        spi_slave_transaction_t * trans_desc;
        if (spi_slave_get_trans_result(VSPI_HOST, &trans_desc, 0) == ESP_OK) {
            *data_len = trans_desc->trans_len / 8;
            recv_data_ready = 0;
            gpio_set_level(SPI_EN, 1);
            return recv_buf;
        }
        recv_data_ready = 0;
        gpio_set_level(SPI_EN, 1);
    }
    return NULL;
}