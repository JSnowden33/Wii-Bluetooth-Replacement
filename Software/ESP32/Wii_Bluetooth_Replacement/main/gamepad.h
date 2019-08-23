#ifndef _GAMEPAD_H_
#define	_GAMEPAD_H_

#define LONG_PRESS_THRESH 700   // 700ms threshold for button events

#include "hid_controller.h"
#include "wiimote.h"

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t x;
    uint8_t y;
    uint8_t du;
    uint8_t dd;
    uint8_t dr;
    uint8_t dl;
    uint8_t r;
    uint8_t zr;
    uint8_t l;
    uint8_t zl;
    uint8_t plus;
    uint8_t minus;
    uint8_t home;
    uint8_t capture;
    uint8_t rs;
    uint8_t ls;
    uint8_t srr;
    uint8_t slr;
    uint8_t srl;
    uint8_t sll;

    // Wiimote-specific buttons
    uint8_t one;
    uint8_t two;
    uint8_t c;
    uint8_t z;
} buttons_t;

typedef struct {
    uint16_t joy_lx;
	uint16_t joy_ly;
    uint16_t joy_rx;
	uint16_t joy_ry;
	int16_t accel_rx;
	int16_t accel_ry;
	int16_t accel_rz;
    int16_t accel_lx;
	int16_t accel_ly;
	int16_t accel_lz;
	int16_t gyro_rx;
	int16_t gyro_ry;
	int16_t gyro_rz;
    int16_t gyro_lx;
	int16_t gyro_ly;
	int16_t gyro_lz;
} axes_t;

typedef struct {
    uint16_t joy_rx_max;
    uint16_t joy_rx_center;
    uint16_t joy_rx_min;
    uint16_t joy_ry_max;
    uint16_t joy_ry_center;
    uint16_t joy_ry_min;
    int16_t accel_rx_offset;
    int16_t accel_ry_offset;
    int16_t accel_rz_offset;
    int16_t gyro_rx_offset;
    int16_t gyro_ry_offset;
    int16_t gyro_rz_offset;
    uint16_t joy_lx_max;
    uint16_t joy_lx_center;
    uint16_t joy_lx_min;
    uint16_t joy_ly_max;
    uint16_t joy_ly_center;
    uint16_t joy_ly_min;
    int16_t accel_lx_offset;
    int16_t accel_ly_offset;
    int16_t accel_lz_offset;
    int16_t gyro_lx_offset;
    int16_t gyro_ly_offset;
    int16_t gyro_lz_offset;
} calibration_t;

typedef struct {
    int32_t press_timer;
    int32_t release_timer;
    uint8_t state;
    uint8_t triggered;
} button_event_t;

enum GAMEPAD_TYPE { GAMEPAD_NONE, 
                    GAMEPAD_JOYCON_R_VERT,  // Single Joy-Con held vertically
                    GAMEPAD_JOYCON_L_VERT, 
                    GAMEPAD_JOYCON_R_SIDE,  // Single Joy-Con held sideways
                    GAMEPAD_JOYCON_L_SIDE,
                    GAMEPAD_JOYCON_DUAL,    // Two Joy-Con as one gamepad
                    GAMEPAD_JOYCON_WIRED,   // Functions similarly to dual
                    GAMEPAD_PROCON, 
                    GAMEPAD_WIIMOTE, 
                    GAMEPAD_WIIU_PRO };
typedef struct {
    enum GAMEPAD_TYPE type;
	buttons_t buttons;
	axes_t axes;
	calibration_t cal;

    button_event_t mode_switch;
    button_event_t gyro_calibrate;
    button_event_t gyro_center;
    button_event_t gyro_enable;

    uint8_t calibrating;
    uint32_t calibration_timer;
    int32_t calibrate_gyro_x_sum; 
    int32_t calibrate_gyro_y_sum; 
    int32_t calibrate_gyro_z_sum;
    uint8_t calibrate_num_samples;

    double yaw;
    double pitch;
    double roll;
    uint64_t imu_timer;

    wiimote_ir_object_t ir1;
    wiimote_ir_object_t ir2;
    wiimote_ir_object_t ir3;
    wiimote_ir_object_t ir4;
} gamepad_t;

extern gamepad_t gamepads[4];

void gamepad_reset(gamepad_t * gamepad);
void init_gamepads();
void gamepad_set_type(uint8_t gamepad_num);
void gamepad_set_extension(uint8_t gamepad_num);
void gamepad_get_angles_from_controller(hid_controller_t * controller);
void gamepad_handle(uint8_t gamepad_num);
void gamepad_set_cal_nunchuk(hid_controller_t * controller);
void gamepad_set_cal_classic(hid_controller_t * controller);
void gamepad_set_cal_imu(hid_controller_t * controller);
void gamepad_set_cal_accel_r(hid_controller_t * controller);
void gamepad_set_cal_accel_l(hid_controller_t * controller);
void gamepad_set_cal_gyro_r(hid_controller_t * controller);
void gamepad_set_cal_gyro_l(hid_controller_t * controller);
void gamepad_set_cal_joy_r(hid_controller_t * controller);  
void gamepad_set_cal_joy_l(hid_controller_t * controller);

#endif