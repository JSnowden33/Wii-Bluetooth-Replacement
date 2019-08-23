#include <inttypes.h>
#include <stdio.h>
#include "btstack_config.h"
#include "btstack.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "rom/ets_sys.h"
#include "esp_system.h"
#include "gamepad.h"
#include "timer.h"
#include "uart_controller.h"
#include "pair.h"

// Subcommand format:
// Header is constant (19 01 03 38 00 92 00 31)
// Bytes 10-11 change rapidly - some kind of timer?
// Byte 12 is "01" for subcommand
// Byte 13 is packet counter (0 - F)
// Bytes 14-21 are rumble (same as Bluetooth subcommand)
// Byte 22 is subcommand ID
// Following bytes are standard subcommand arguments

uint8_t uart_handshake_commands[15][64] = {
    { 16,   0xA1, 0xA2, 0xA3, 0xA4, 0x19, 0x01, 0x03, 0x07, 0x00, 0xA5, 0x02, 0x01,	0x7E, 0x00, 0x00, 0x00 },	// Handshake 1
    { 12,   0x19, 0x01, 0x03, 0x07, 0x00, 0x91, 0x01, 0x00, 0x00, 0x00, 0x00, 0x24 },	// Get MAC address
    { 16,   0x19, 0x01, 0x03, 0x0B, 0x00, 0x91, 0x02, 0x04, 0x00, 0x00, 0x38, 0xE6, 0x14, 0x00, 0x00, 0x00 },	// Handshake 2
    { 20,   0x19, 0x01, 0x03, 0x0F, 0x00, 0x91, 0x20, 0x08, 0x00, 0x00, 0xBD, 0xB1, 0xC0, 0xC6, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00 },	// Switch baud rate
    { 12,   0x19, 0x01, 0x03, 0x07, 0x00, 0x91, 0x11, 0x00, 0x00, 0x00, 0x00, 0x0E }, 	// Pair 1
    { 12,   0x19, 0x01, 0x03, 0x07, 0x00, 0x91, 0x10, 0x00, 0x00, 0x00, 0x00, 0x3D }, 	// Pair 2
    { 16,   0x19, 0x01, 0x03, 0x0B, 0x00, 0x91, 0x12, 0x04, 0x00, 0x00, 0x12, 0xA6, 0x0F, 0x00, 0x00, 0x00 },	// Pair 3
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Read IMU calibration
      0x01, 0x04, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x10, 0x20, 
      0x60, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Read right joystick calibration
      0x01, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x10, 0x3D, 
      0x60, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Read left joystick calibration
      0x01, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x10, 0x46, 
      0x60, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Begin pairing
      0x01, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x01, 0x01, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Get pair key
      0x01, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x01, 0x02, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Save pair key
      0x01, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x01, 0x03, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
	{ 61,   0x19, 0x01, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,   	// Enable IMU
      0x01, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40, 0x40, 0x01, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00 },
    { 13,   0x19, 0x01, 0x03, 0x08, 0x00, 0x92, 0x00, 0x01, 0x00, 0x00, 0x69, 0x2D, 0x1F }	// Get status
};

const uint8_t uart_response_baud_switched[12] = { 0x19, 0x81, 0x03, 0x07, 0x00, 0x94, 0x20, 0x00, 0x00, 0x00, 0x00, 0xA8 };
const uint8_t uart_mac_response_header[8] = { 0x19, 0x81, 0x03, 0x0F, 0x00, 0x94, 0x01, 0x08 };
const uint8_t uart_status_response_header[8] = { 0x19, 0x81, 0x03, 0x38, 0x00, 0x92, 0x00, 0x31 };	// Header for long responses

uart_joycon_t joycon_right;
uart_joycon_t joycon_left;
gpio_config_t pin_config; 

static void uart_joycon_write_next_command(uart_joycon_t * joycon) {
	uint8_t i;
	if (joycon->handshake_pos == 10) {
		uart_handshake_commands[joycon->handshake_pos][25] = host_mac_addr[5];
		uart_handshake_commands[joycon->handshake_pos][26] = host_mac_addr[4];
		uart_handshake_commands[joycon->handshake_pos][27] = host_mac_addr[3];
		uart_handshake_commands[joycon->handshake_pos][28] = host_mac_addr[2];
		uart_handshake_commands[joycon->handshake_pos][29] = host_mac_addr[1];
		uart_handshake_commands[joycon->handshake_pos][30] = host_mac_addr[0];
	}
	for (i = 1; i <= uart_handshake_commands[joycon->handshake_pos][0]; i++) joycon->uart_port->fifo.rw_byte = uart_handshake_commands[joycon->handshake_pos][i];
	if (joycon->handshake_pos < 14) joycon->handshake_pos++;
}

static void uart_joycon_intr_handle(uart_joycon_t * joycon) {
	if (!joycon->chars_remaining) gpio_set_level(joycon->rx_en_pin, 0);
	uart_disable_rx_intr(joycon->uart_num);
	uart_clear_intr_status(joycon->uart_num, UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
	
	uint16_t rx_fifo_len;
	rx_fifo_len = joycon->uart_port->status.rxfifo_cnt;

	if (!joycon->chars_remaining) {
		joycon->message_started = 1;
		joycon->message_len = 0;
	} else if (rx_fifo_len >= joycon->chars_remaining) {
		joycon->message_started = 0;	// Message ends when remaining chars are received
		joycon->chars_remaining = 0;
	}
	
	// Read data
	while(rx_fifo_len){
		joycon->rx_buf[joycon->message_len] = joycon->uart_port->fifo.rw_byte;
		joycon->message_len++;
		rx_fifo_len--;
	}

	if (joycon->message_started) {
		joycon->chars_remaining = joycon->rx_buf[3] + 5 - joycon->message_len;
		if (joycon->chars_remaining > 0) joycon->uart_intr.rxfifo_full_thresh = joycon->chars_remaining;
		else joycon->chars_remaining = 8;	// Assuming 8 chars remaining
	} else {
		joycon->message_received = 1;
		joycon->uart_intr.rxfifo_full_thresh = 4;	// Wait for next command header
		if (!gpio_get_level(joycon->tx_en_pin)) uart_joycon_write_next_command(joycon);
		else joycon->command_queued = 1;
		if (!memcmp(joycon->rx_buf, uart_response_baud_switched, 12)) joycon->baud_switch_queued = 1;
	}
	uart_intr_config(joycon->uart_num, &joycon->uart_intr);
	uart_enable_rx_intr(joycon->uart_num);

	gpio_set_level(joycon->rx_en_pin, 1);
}

static void joycon_right_intr_handle(void *arg) {
	uart_joycon_intr_handle(&joycon_right);
}

static void joycon_left_intr_handle(void *arg) {
	uart_joycon_intr_handle(&joycon_left);
}

static void uart_joycon_setup(uart_joycon_t * joycon) {
	gpio_set_direction(joycon->tx_en_pin, GPIO_MODE_INPUT);
	gpio_set_pull_mode(joycon->tx_en_pin, GPIO_PULLUP_ONLY);
	gpio_set_direction(joycon->rx_en_pin, GPIO_MODE_OUTPUT);
	gpio_set_level(joycon->rx_en_pin, 1);

	joycon->uart_config.baud_rate = 1000000;
    joycon->uart_config.data_bits = UART_DATA_8_BITS;
    joycon->uart_config.parity = UART_PARITY_DISABLE;
    joycon->uart_config.stop_bits = UART_STOP_BITS_2;
    joycon->uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uart_param_config(joycon->uart_num, &joycon->uart_config);
	uart_set_pin(joycon->uart_num, joycon->tx_pin, joycon->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(joycon->uart_num, UART_INVERSE_TXD);

	uart_driver_install(joycon->uart_num, 2048, 0, 0, NULL, 0);
	uart_isr_free(joycon->uart_num);
	if (joycon->type == UART_JOYCON_R) uart_isr_register(joycon->uart_num, joycon_right_intr_handle, NULL, ESP_INTR_FLAG_LOWMED, NULL);
	else if (joycon->type == UART_JOYCON_L) uart_isr_register(joycon->uart_num, joycon_left_intr_handle, NULL, ESP_INTR_FLAG_LOWMED, NULL);

	joycon->uart_intr.intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M
									   | UART_RXFIFO_TOUT_INT_ENA_M
									   | UART_FRM_ERR_INT_ENA_M
									   | UART_RXFIFO_OVF_INT_ENA_M
									   | UART_BRK_DET_INT_ENA_M
									   | UART_PARITY_ERR_INT_ENA_M;
    joycon->uart_intr.rxfifo_full_thresh = 4;
    joycon->uart_intr.rx_timeout_thresh = 10;
    joycon->uart_intr.txfifo_empty_intr_thresh = 10;
	uart_intr_config(joycon->uart_num, &joycon->uart_intr);
	uart_enable_rx_intr(joycon->uart_num);

	joycon->command_queued = 1;
	joycon->connected = 0;
}

void uart_set_mac_address(uart_joycon_t * joycon) {
	if (!joycon->address_stored) {
		joycon->address[0] = joycon->rx_buf[33];
		joycon->address[1] = joycon->rx_buf[32];
		joycon->address[2] = joycon->rx_buf[31];
		joycon->address[3] = joycon->rx_buf[30];
		joycon->address[4] = joycon->rx_buf[29];
		joycon->address[5] = joycon->rx_buf[28];
		printf("MAC address: ");
		printf_hexdump(joycon->address, 6);
		joycon->address_stored = 1;
	}
}

void uart_store_link_key(uart_joycon_t * joycon) {
	if (!joycon->key_stored) {
		uint8_t i;
		uint8_t link_key[16];
		for (i = 0; i < 16; i++) link_key[i] = joycon->rx_buf[43 - i] ^ 0xAA;	// Read link key little-endian, XOR each byte with 0xAA
		printf("Link key: ");
		printf_hexdump(link_key, 16);
		gap_store_link_key_for_bd_addr(joycon->address, link_key, LOCAL_UNIT_KEY);
		joycon->key_stored = 1;
	}
}

void uart_set_cal_imu(uart_joycon_t * joycon) {
	joycon->accel_x_offset = (joycon->rx_buf[33] << 8) | joycon->rx_buf[32];
	joycon->accel_y_offset = (joycon->rx_buf[35] << 8) | joycon->rx_buf[34];
	joycon->accel_z_offset = (joycon->rx_buf[37] << 8) | joycon->rx_buf[36];
	joycon->gyro_x_offset = (joycon->rx_buf[45] << 8) | joycon->rx_buf[44];
	joycon->gyro_y_offset = (joycon->rx_buf[47] << 8) | joycon->rx_buf[46];
	joycon->gyro_z_offset = (joycon->rx_buf[49] << 8) | joycon->rx_buf[48];
	printf("Accel X offset: %d\n", joycon->accel_x_offset);
	printf("Accel Y offset: %d\n", joycon->accel_y_offset);
	printf("Accel Z offset: %d\n", joycon->accel_z_offset);
	printf("Gyro X offset: %d\n", joycon->gyro_x_offset);
	printf("Gyro Y offset: %d\n", joycon->gyro_y_offset);
	printf("Gyro Z offset: %d\n", joycon->gyro_z_offset);
}

void uart_set_cal_joy(uart_joycon_t * joycon) {
	switch (joycon->type) {
		case UART_JOYCON_L:
			joycon->joy_x_center = ((joycon->rx_buf[36] << 8) & 0xF00) | joycon->rx_buf[35];
            joycon->joy_y_center = (joycon->rx_buf[37] << 4) | (joycon->rx_buf[36] >> 4);
            joycon->joy_x_min = joycon->joy_x_center - (((joycon->rx_buf[39] << 8) & 0xF00) | joycon->rx_buf[38]);
            joycon->joy_x_max = joycon->joy_x_center + (((joycon->rx_buf[33] << 8) & 0xF00) | joycon->rx_buf[32]);
            joycon->joy_y_min = joycon->joy_y_center - ((joycon->rx_buf[40] << 4) | (joycon->rx_buf[39] >> 4));
            joycon->joy_y_max = joycon->joy_y_center + ((joycon->rx_buf[34] << 4) | (joycon->rx_buf[33] >> 4));
			break;
		case UART_JOYCON_R:
			joycon->joy_x_center = ((joycon->rx_buf[33] << 8) & 0xF00) | joycon->rx_buf[32];
            joycon->joy_y_center = (joycon->rx_buf[34] << 4) | (joycon->rx_buf[33] >> 4);
            joycon->joy_x_min = joycon->joy_x_center - (((joycon->rx_buf[36] << 8) & 0xF00) | joycon->rx_buf[35]);
            joycon->joy_x_max = joycon->joy_x_center + (((joycon->rx_buf[39] << 8) & 0xF00) | joycon->rx_buf[38]);
            joycon->joy_y_min = joycon->joy_y_center - ((joycon->rx_buf[37] << 4) | (joycon->rx_buf[38] >> 4));
            joycon->joy_y_max = joycon->joy_y_center + ((joycon->rx_buf[40] << 4) | (joycon->rx_buf[39] >> 4));
			break;
	}
	printf("Joystick X center: %d\n", joycon->joy_x_center);
	printf("Joystick Y center: %d\n", joycon->joy_y_center);
	printf("Joystick X min: %d\n", joycon->joy_x_min);
	printf("Joystick X max: %d\n", joycon->joy_x_max);
	printf("Joystick Y min: %d\n", joycon->joy_y_min);
	printf("Joystick Y max: %d\n", joycon->joy_y_max);
}

void reset_joycon(uart_joycon_t * joycon) {
	memset(joycon->address, 0, 6);
    joycon->address_stored = 0;
    joycon->key_stored = 0;

	memset(joycon->rx_buf, 0, 256);
    joycon->handshake_pos = 0;
    joycon->chars_remaining = 0;
    joycon->message_started = 0;
    joycon->message_received = 0;
    joycon->message_len = 0;
    joycon->command_queued = 0;
    joycon->baud_switch_queued = 0;
    joycon->connected = 0;
    joycon->connect_timer = 0;
    joycon->data_ready = 0;

    joycon->joy_x_max = 0;
    joycon->joy_x_center = 0;
    joycon->joy_x_min = 0;
    joycon->joy_y_max = 0;
    joycon->joy_y_center = 0;
    joycon->joy_y_min = 0;
    joycon->accel_x_offset = 0;
    joycon->accel_y_offset = 0;
    joycon->accel_z_offset = 0;
    joycon->gyro_x_offset = 0;
    joycon->gyro_y_offset = 0;
    joycon->gyro_z_offset = 0;
}

void uart_joycon_handle(uart_joycon_t * joycon) {
	if (joycon->connected) {
		if (joycon->message_received) {
			// Check responses and get data
			if (!memcmp(joycon->rx_buf + 26, uart_handshake_commands[7] + 23, 6)) uart_set_cal_imu(joycon);
			if (!memcmp(joycon->rx_buf + 26, uart_handshake_commands[8] + 23, 6) && joycon->type == UART_JOYCON_L) uart_set_cal_joy(joycon);
			if (!memcmp(joycon->rx_buf + 26, uart_handshake_commands[9] + 23, 6) && joycon->type == UART_JOYCON_R) uart_set_cal_joy(joycon);
			if (!memcmp(joycon->rx_buf + 26, uart_handshake_commands[10] + 23, 2)) uart_set_mac_address(joycon);
			if (!memcmp(joycon->rx_buf + 26, uart_handshake_commands[11] + 23, 2)) uart_store_link_key(joycon);

			if (!memcmp(joycon->rx_buf, uart_status_response_header, 8)) joycon->data_ready = 1;
			else joycon->data_ready = 0;

			uart_flush(joycon->uart_num);
			joycon->message_received = 0;
			joycon->connect_timer = app_timer;
		}
		if (joycon->command_queued && !gpio_get_level(joycon->tx_en_pin)) {
			uart_joycon_write_next_command(joycon);
			joycon->command_queued = 0;
		}

		if (joycon->baud_switch_queued) {
			uart_set_baudrate(joycon->uart_num, 3125000);
			printf("Baud switched to 3.125 Mbps\n");
			joycon->baud_switch_queued = 0;
		}

		if (((app_timer - joycon->connect_timer) % 200) == 0) {
			// Resend last status request
		}

		if ((app_timer - joycon->connect_timer > 500) && gpio_get_level(joycon->tx_en_pin)) {	// If 500ms has passed without communication
			printf("Joy-Con disconnected\n");
			uart_set_baudrate(joycon->uart_num, 1000000);
			reset_joycon(joycon);
			uart_flush(joycon->uart_num);
			joycon->uart_intr.rxfifo_full_thresh = 4;
			uart_enable_rx_intr(joycon->uart_num);
		}
	} else {
		if (!gpio_get_level(joycon->tx_en_pin)) {	// This line goes low in a Joy-Con connection event
			printf("Joy-Con connected\n");
			joycon->connected = 1;
			joycon->command_queued = 1;
			joycon->connect_timer = app_timer;	// Store time of connection
			uart_flush(joycon->uart_num);
			joycon->uart_intr.rxfifo_full_thresh = 4;
			uart_enable_rx_intr(joycon->uart_num);
		}
	}
}

void uart_init() {
	reset_joycon(&joycon_right);
	reset_joycon(&joycon_left);
	
	joycon_right.uart_num = UART_NUM_1;
	joycon_right.uart_port = &UART1;
	joycon_right.tx_pin = JOYCON_R_TX;
	joycon_right.rx_pin = JOYCON_R_RX;
	joycon_right.rx_en_pin = JOYCON_R_RX_EN;
	joycon_right.tx_en_pin = JOYCON_R_TX_EN;
	joycon_right.type = UART_JOYCON_R;

	joycon_left.uart_num = UART_NUM_2;
	joycon_left.uart_port = &UART2;
	joycon_left.tx_pin = JOYCON_L_TX;
	joycon_left.rx_pin = JOYCON_L_RX;
	joycon_left.rx_en_pin = JOYCON_L_RX_EN;
	joycon_left.tx_en_pin = JOYCON_L_TX_EN;
	joycon_left.type = UART_JOYCON_L;

	// TODO: Add check to see if Joy-Con is paired with ESP32 and ESP32 is paired with Joy-Con, and don't pair if already paired
	// Create proper command queue
	// Add rumble and gyro angle calculation

	uart_joycon_setup(&joycon_right);
	uart_joycon_setup(&joycon_left);
}