#ifndef _UART_DRIVER_H_
#define	_UART_DRIVER_H_

#include "driver/uart.h"

// TX means sending from ESP32, RX means ESP32 receiving
#define JOYCON_R_TX	    GPIO_NUM_32
#define JOYCON_R_RX	    GPIO_NUM_39
#define JOYCON_R_RX_EN	GPIO_NUM_33	// Console controls this line (High = Joy-Con can send, Low = Joy-Con cannot send)
#define JOYCON_R_TX_EN	GPIO_NUM_36	// Joy-Con controls this line (High = Console cannot send, Low = Console can send)
#define JOYCON_L_TX	    GPIO_NUM_16
#define JOYCON_L_RX	    GPIO_NUM_34
#define JOYCON_L_RX_EN	GPIO_NUM_4	
#define JOYCON_L_TX_EN	GPIO_NUM_35	

#define UART_GAMEPAD_NUM 1  // Define which gamepad the wired controllers are bound to

enum JOYCON_TYPE { UART_JOYCON_R, UART_JOYCON_L };
typedef struct {
    enum JOYCON_TYPE type;
    uint8_t address[6];
    uint8_t address_stored;
    uint8_t key_stored;

    uint8_t uart_num;
    uart_dev_t * uart_port;
    uart_config_t uart_config;
    uart_intr_config_t uart_intr;

    uint8_t rx_buf[256];
    uint8_t handshake_pos;
    int8_t chars_remaining;
    uint8_t message_started;
    uint8_t message_received;
    uint8_t message_len;
    uint8_t command_queued;
    uint8_t baud_switch_queued;
    uint8_t connected;
    uint32_t connect_timer;
    uint8_t data_ready;

    uint64_t tx_pin;
    uint64_t rx_pin;
    uint64_t rx_en_pin;  
    uint64_t tx_en_pin;  

    // Calibration data
    uint16_t joy_x_max;
    uint16_t joy_x_center;
    uint16_t joy_x_min;
    uint16_t joy_y_max;
    uint16_t joy_y_center;
    uint16_t joy_y_min;
    int16_t accel_x_offset;
    int16_t accel_y_offset;
    int16_t accel_z_offset;
    int16_t gyro_x_offset;
    int16_t gyro_y_offset;
    int16_t gyro_z_offset;
} uart_joycon_t;

extern uart_joycon_t joycon_right;
extern uart_joycon_t joycon_left;

void uart_joycon_handle(uart_joycon_t * joycon);
void uart_init();

#endif