#ifndef _WIIMOTE_H_
#define _WIIMOTE_H_

#include "hid_controller.h"

// Half distance between 2 IR dots
#define IR_OFFSET   50

// Coordinates based on calibration in Wiimote EEPROM on PIC32 (sensor bar below TV)
#define IR_X_MIN    112.0
#define IR_X_CENTER 512.0
#define IR_X_MAX    912.0
 
#define IR_Y_MIN    84.0
#define IR_Y_CENTER 284.0
#define IR_Y_MAX    484.0

// Max angles allowed from center of screen
#define IR_PITCH_MAX    12.0
#define IR_YAW_MAX      20.0

#define IR_JOY_SENSITIVITY  10.0
#define IR_JOY_DEADZONE     400 	// Joystick range is 0 to 4095
#define IR_JOY_IDLE_TIME    1700 	// Time (ms) of no joystick movement required before cursor becomes idle

#define DPAD_JOY_DEADZONE	800

typedef struct {
    double x;
    double y;
    uint8_t size;
    uint8_t xmin;
    uint8_t ymin;
    uint8_t xmax;
    uint8_t ymax;
    uint8_t intensity;
} wiimote_ir_object_t;

typedef struct {
    uint16_t accel_x;
    uint16_t accel_y;
    uint16_t accel_z;
    uint8_t x;
    uint8_t y;
    uint8_t c;
    uint8_t z;
} wiimote_nunchuk_t;

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t x;
    uint8_t y;
    uint8_t du;
    uint8_t dd;
    uint8_t dr;
    uint8_t dl;
    uint8_t plus;
    uint8_t minus;
    uint8_t home;
    uint8_t r;
    uint8_t l;
    uint8_t zr;
    uint8_t zl;
    uint8_t rt;   // Analog trigger
    uint8_t lt;   // Analog trigger
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
} wiimote_classic_t;

typedef struct {
    uint16_t yaw_speed;  // 14-bit resolution
    uint16_t pitch_speed;
    uint16_t roll_speed;
    uint8_t yaw_slow;
    uint8_t pitch_slow;
    uint8_t roll_slow;
} wiimote_motion_plus_t;

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t one;
    uint8_t two;
    uint8_t du;
    uint8_t dd;
    uint8_t dr;
    uint8_t dl;
    uint8_t plus;
    uint8_t minus;
    uint8_t home;

    // Accelerometer (10 bit range)
    uint16_t accel_x;
    uint16_t accel_y;
    uint16_t accel_z;

    wiimote_ir_object_t ir_object[2];   // 2 IR camera dots
    wiimote_nunchuk_t nunchuk;
    wiimote_classic_t classic;
    wiimote_motion_plus_t motion_plus;

    uint8_t ir_idle;    // IR data will not be used if this is set
    uint32_t ir_idle_timeout; // Makes the cursor disappear if joystick is not moved

    uint8_t active;
    uint8_t player_num; // Based on LED pattern
    uint8_t rumble;

    uint8_t battery_level;

    enum EXTENSION_TYPE extension;
} wiimote_t;

extern wiimote_t wiimotes[4];

void wiimote_reset(wiimote_t * wiimote);
void init_wiimotes();
void wiimote_set_extension(uint8_t wiimote_num, enum EXTENSION_TYPE extension);
void wiimote_change_extension(uint8_t wiimote_num);
void wiimote_handle(uint8_t wiimote_num);
void wiimote_spi_send(uint8_t wiimote_num);
void wiimote_spi_receive();

#endif