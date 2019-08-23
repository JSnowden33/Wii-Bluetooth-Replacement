#include <inttypes.h>
#include <stdio.h>
#include "btstack.h"
#include "wiimote.h"
#include "gamepad.h"
#include "timer.h"
#include "spi.h"
#include "hid_controller.h"

wiimote_t wiimotes[4];

static double map(double x, double in_min, double in_max, double out_min, double out_max) {
    return ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

static void wiimote_set_defaults(wiimote_t * wiimote) {
    wiimote->a = 0;
    wiimote->b = 0;
    wiimote->one = 0;
    wiimote->two = 0;
    wiimote->du = 0;
    wiimote->dd = 0;
    wiimote->dr = 0;
    wiimote->dl = 0;
    wiimote->plus = 0;
    wiimote->minus = 0;
    wiimote->home = 0;

    wiimote->accel_x = 0;
    wiimote->accel_y = 0;
    wiimote->accel_z = 0x80;

    wiimote->battery_level = 0xFF;

    // TODO: Set extension data to defaults
}

static void wiimote_angle_to_ir(wiimote_t * wiimote, gamepad_t * gamepad) {
    wiimote->ir_object[0].x = map(-gamepad->yaw, -IR_YAW_MAX, IR_YAW_MAX, IR_X_MIN, IR_X_MAX) + IR_OFFSET;
    wiimote->ir_object[0].y = map(-gamepad->pitch, -IR_PITCH_MAX, IR_PITCH_MAX, IR_Y_MIN, IR_Y_MAX);
    wiimote->ir_object[1].x = map(-gamepad->yaw, -IR_YAW_MAX, IR_YAW_MAX, IR_X_MIN, IR_X_MAX) - IR_OFFSET;
    wiimote->ir_object[1].y = map(-gamepad->pitch, -IR_PITCH_MAX, IR_PITCH_MAX, IR_Y_MIN, IR_Y_MAX);

    if ((gamepad->yaw < -IR_YAW_MAX - 5) || (gamepad->yaw > IR_YAW_MAX + 5) ||
        (gamepad->pitch < -IR_PITCH_MAX - 5) || (gamepad->pitch > IR_PITCH_MAX + 5))
        wiimote->ir_idle = 1;
    else {
        wiimote->ir_idle = 0;
        wiimote->ir_idle_timeout = app_timer;
    }
	
	// TODO: Allow offsets so gyro cursor can keep up with joystick cursor
}

static uint8_t wiimote_joy_to_ir(wiimote_t * wiimote, gamepad_t * gamepad) {
    if (gamepad->buttons.r) {
        wiimote->ir_object[0].x = IR_X_CENTER + IR_OFFSET;
        wiimote->ir_object[0].y = IR_Y_CENTER;
        wiimote->ir_object[1].x = IR_X_CENTER - IR_OFFSET;
        wiimote->ir_object[1].y = IR_Y_CENTER;
        wiimote->ir_idle = 0;
        wiimote->ir_idle_timeout = app_timer;
    }
    
    uint32_t joy_x_sq = ((int16_t)gamepad->axes.joy_rx - 0x7FF) * ((int16_t)gamepad->axes.joy_rx - 0x7FF);
    uint32_t joy_y_sq = ((int16_t)gamepad->axes.joy_ry - 0x7FF) * ((int16_t)gamepad->axes.joy_ry - 0x7FF);
    uint32_t deadzone_sq = IR_JOY_DEADZONE * IR_JOY_DEADZONE;
    
    if (joy_x_sq + joy_y_sq > deadzone_sq) {    // Joystick is outside deadzone
        double x_speed = ((int16_t)gamepad->axes.joy_rx - 0x7FF) * IR_JOY_SENSITIVITY / 0xFFF;
        double y_speed = ((int16_t)gamepad->axes.joy_ry - 0x7FF) * IR_JOY_SENSITIVITY / 0xFFF;
        
        wiimote->ir_object[0].x -= x_speed;
        wiimote->ir_object[0].y -= y_speed;
        wiimote->ir_object[1].x -= x_speed;
        wiimote->ir_object[1].y -= y_speed;

        wiimote->ir_idle = 0;
        wiimote->ir_idle_timeout = app_timer;

        // Keep joystick cursor within bounds
        if (wiimote->ir_object[0].x > 0x3FF) wiimote->ir_object[0].x = 0x3FF;
        if (wiimote->ir_object[0].x < 0) wiimote->ir_object[0].x = 0;
        if (wiimote->ir_object[0].y > 0x3FF) wiimote->ir_object[0].y = 0x3FF;
        if (wiimote->ir_object[0].y < 0) wiimote->ir_object[0].y = 0;
        if (wiimote->ir_object[1].x > 0x3FF) wiimote->ir_object[1].x = 0x3FF;
        if (wiimote->ir_object[1].x < 0) wiimote->ir_object[1].x = 0;
        if (wiimote->ir_object[1].y > 0x3FF) wiimote->ir_object[1].y = 0x3FF;
        if (wiimote->ir_object[1].y < 0) wiimote->ir_object[1].y = 0;

        return 1;
    } else {
        if (!wiimote->ir_idle && ((app_timer - wiimote->ir_idle_timeout) >= IR_JOY_IDLE_TIME)) wiimote->ir_idle = 1;
        return 0;
    }
}

static void wiimote_update_ir(wiimote_t * wiimote, gamepad_t * gamepad) {
    switch(gamepad->type) {
        case GAMEPAD_JOYCON_R_VERT:
            wiimote_angle_to_ir(wiimote, gamepad);
            break;
        case GAMEPAD_JOYCON_L_VERT:
            wiimote->ir_idle = 1;
            break;
        case GAMEPAD_JOYCON_DUAL:
        case GAMEPAD_PROCON:
            if (wiimote->extension != EXT_CLASSIC) {
                if (!wiimote_joy_to_ir(wiimote, gamepad)) wiimote_angle_to_ir(wiimote, gamepad);    // Use angles if joystick is inactive
            }
            else wiimote->ir_idle = 1;
            break;
        case GAMEPAD_WIIMOTE:
            wiimote_angle_to_ir(wiimote, gamepad);
            // TODO: Pass through actual IR data if gyro is disabled
            break;
        case GAMEPAD_WIIU_PRO:
            wiimote_joy_to_ir(wiimote, gamepad);
            break;
        default:
            break;
    }
}

void wiimote_reset(wiimote_t * wiimote) {
    memset(wiimote, 0, sizeof(wiimote_t));
    wiimote->extension = EXT_NONE;
    wiimote_set_defaults(wiimote);
}

void init_wiimotes() {
    wiimote_reset(&wiimotes[0]);
    wiimote_reset(&wiimotes[1]);
    wiimote_reset(&wiimotes[2]);
    wiimote_reset(&wiimotes[3]);
}

void wiimote_set_extension(uint8_t wiimote_num, enum EXTENSION_TYPE extension) {
    if (wiimote_num > 0 && wiimote_num <= 4) wiimotes[wiimote_num - 1].extension = extension;
}

void wiimote_change_extension(uint8_t wiimote_num) {
    // Cycle through virtual extensions based on gamepad type
    gamepad_t * gamepad = &gamepads[wiimote_num - 1];
    wiimote_t * wiimote = &wiimotes[wiimote_num - 1];
    switch (gamepad->type) {
        case GAMEPAD_JOYCON_DUAL:
            if (wiimote->extension == EXT_NUNCHUK) wiimote->extension = EXT_CLASSIC;
            else if (wiimote->extension == EXT_CLASSIC) wiimote->extension = EXT_NUNCHUK;
            else wiimote->extension = EXT_NUNCHUK;
            break;
        case GAMEPAD_PROCON:
        case GAMEPAD_JOYCON_WIRED:
        case GAMEPAD_WIIU_PRO:
            wiimote->extension++;
            wiimote->extension = wiimote->extension % 3;
            break;
        default:
            break;
    }
    printf("Extension switched\n");
}

// GAMEPAD_JOYCON_R_SIDE: The only extension option in this mode is EXT_NONE (sideways Wiimote)
// GAMEPAD_JOYCON_L_SIDE: The only extension option in this mode is EXT_NONE (sideways Wiimote)
// GAMEPAD_JOYCON_R_VERT: The only extension option in this mode is EXT_NONE (vertical Wiimote)
// GAMEPAD_JOYCON_L_VERT: The only extension option in this mode is EXT_NUNCHUK
// GAMEPAD_JOYCON_DUAL: The extension options in this mode are EXT_NUNCHUK and EXT_CLASSIC
// GAMEPAD_PROCON: The extension options in this mode are EXT_NONE (sideways Wiimote), EXT_NUNCHUK, and EXT_CLASSIC
// GAMEPAD_WIIU_PRO: The extension options in this mode are EXT_NONE (sideways Wiimote), EXT_NUNCHUK, and EXT_CLASSIC (no motion)
// GAMEPAD_WIIMOTE: The extension options in this mode are the same as the extension physically plugged in

void wiimote_handle(uint8_t wiimote_num) {
    gamepad_t * gamepad = &gamepads[wiimote_num - 1];
    wiimote_t * wiimote = &wiimotes[wiimote_num - 1];
    wiimote_set_defaults(wiimote);

    if (wiimote->extension == EXT_CLASSIC) {
        wiimote->plus = gamepad->buttons.plus;
        wiimote->minus = gamepad->buttons.minus;
        wiimote->home = gamepad->buttons.home;
    } else {
        wiimote->a = gamepad->buttons.a;
        wiimote->b = gamepad->buttons.b;
        wiimote->one = gamepad->buttons.y;
        wiimote->two = gamepad->buttons.x;
        wiimote->du = gamepad->buttons.du;
        wiimote->dd = gamepad->buttons.dd;
        wiimote->dr = gamepad->buttons.dr;
        wiimote->dl = gamepad->buttons.dl;
        wiimote->plus = gamepad->buttons.plus;
        wiimote->plus |= gamepad->buttons.sll;
        wiimote->minus = gamepad->buttons.minus;
        wiimote->minus |= gamepad->buttons.srr;
        wiimote->home = gamepad->buttons.home;
    }

    wiimote->nunchuk.accel_x = (gamepad->axes.accel_lx + 0x8000) >> 6;
    wiimote->nunchuk.accel_y = (gamepad->axes.accel_ly + 0x8000) >> 6;
    wiimote->nunchuk.accel_z = (gamepad->axes.accel_lz + 0x8000) >> 6;
    wiimote->nunchuk.x = gamepad->axes.joy_lx >> 4;
    wiimote->nunchuk.y = gamepad->axes.joy_ly >> 4;
    wiimote->nunchuk.c = gamepad->buttons.l;
    wiimote->nunchuk.z = gamepad->buttons.zl;
    
    wiimote->classic.a = gamepad->buttons.a;
    wiimote->classic.b = gamepad->buttons.b;
    wiimote->classic.x = gamepad->buttons.x;
    wiimote->classic.y = gamepad->buttons.y;
    wiimote->classic.du = gamepad->buttons.du;
    wiimote->classic.dd = gamepad->buttons.dd;
    wiimote->classic.dr = gamepad->buttons.dr;
    wiimote->classic.dl = gamepad->buttons.dl;
    wiimote->classic.plus = gamepad->buttons.plus;
    wiimote->classic.minus = gamepad->buttons.minus;
    wiimote->classic.home = gamepad->buttons.home;
    wiimote->classic.r = gamepad->buttons.r;
    wiimote->classic.l = gamepad->buttons.l;
    wiimote->classic.zr = gamepad->buttons.zr;
    wiimote->classic.zl = gamepad->buttons.zl;
    wiimote->classic.rt = 0;    // Not sure if 0 is open or fully pressed
    wiimote->classic.lt = 0;
    wiimote->classic.lx = gamepad->axes.joy_lx >> 4;
    wiimote->classic.ly = gamepad->axes.joy_ly >> 4;
    wiimote->classic.rx = gamepad->axes.joy_rx >> 4;
    wiimote->classic.ry = gamepad->axes.joy_ry >> 4;

    wiimote->accel_x = (gamepad->axes.accel_rx + 0x8000) >> 6;
    wiimote->accel_y = (gamepad->axes.accel_ry + 0x8000) >> 6;
    wiimote->accel_z = (gamepad->axes.accel_rz + 0x8000) >> 6;
    wiimote->motion_plus.yaw_speed = (gamepad->axes.gyro_rz + 0x8000) >> 2;
    wiimote->motion_plus.pitch_speed = (gamepad->axes.gyro_ry + 0x8000) >> 2;
    wiimote->motion_plus.roll_speed = (gamepad->axes.gyro_rx + 0x8000) >> 2;
    
    // Adjustments based on gamepad type
    switch (gamepad->type) {
        case GAMEPAD_JOYCON_R_VERT:
            // TODO: Use joystick as d-pad
            break;
        case GAMEPAD_JOYCON_R_SIDE:
            // Use joystick as d-pad
            break;
        case GAMEPAD_JOYCON_L_SIDE:
            // Use joystick as d-pad
            break;
        case GAMEPAD_WIIU_PRO:
            // Neutral motion values
            wiimote->accel_x = 0x200;
            wiimote->accel_y = 0x200;
            wiimote->accel_z = 0x260;
            wiimote->nunchuk.accel_x = 0x200;
            wiimote->nunchuk.accel_y = 0x200;
            wiimote->nunchuk.accel_z = 0x260;
        case GAMEPAD_PROCON:
            if (wiimote->extension == EXT_NONE) {
                // Switch accel x and y for sideways Wiimote 
                wiimote->accel_x = (gamepad->axes.accel_ry + 0x8000) >> 6;
                wiimote->accel_y = (gamepad->axes.accel_rx + 0x8000) >> 6;

                // Rotate d-pad for sideways wiimote
                wiimote->du = gamepad->buttons.dl;
                wiimote->dd = gamepad->buttons.dr;
                wiimote->dr = gamepad->buttons.du;
                wiimote->dl = gamepad->buttons.dd;

                // Use joystick as d-pad
            }
            break;
        case GAMEPAD_WIIMOTE:
            wiimote->one = gamepad->buttons.one;
            wiimote->two = gamepad->buttons.two;
            wiimote->nunchuk.c = gamepad->buttons.c;
            wiimote->nunchuk.z = gamepad->buttons.z;
            break;
        default:
            break;
    }

    wiimote_update_ir(wiimote, gamepad);
}

void wiimote_spi_send(uint8_t wiimote_num) {
    wiimote_t * wiimote = &wiimotes[wiimote_num - 1];
    uint8_t buf[32] = { 0 };

    if (wiimote->active) {
        buf[0] = (wiimote->extension << 4) | wiimote_num;

        buf[1] = wiimote->a | 
                (wiimote->b << 1) |
                (wiimote->one << 2) | 
                (wiimote->two << 3) |
                (wiimote->du << 4) | 
                (wiimote->dd << 5) | 
                (wiimote->dr << 6) | 
                (wiimote->dl << 7);
        
        buf[2] = wiimote->plus | 
                (wiimote->minus << 1) |
                (wiimote->home << 2) | 
                (wiimote->classic.plus << 3) | 
                (wiimote->classic.minus << 4) | 
                (wiimote->classic.home << 5) |
                (wiimote->nunchuk.c << 6) | 
                (wiimote->nunchuk.z << 7);
        buf[3] = wiimote->classic.a | 
                (wiimote->classic.b << 1) |
                (wiimote->classic.x << 2) | 
                (wiimote->classic.y << 3) | 
                (wiimote->classic.du << 4) | 
                (wiimote->classic.dd << 5) |
                (wiimote->classic.dr << 6) | 
                (wiimote->classic.dl << 7);
        buf[4] = wiimote->classic.r | 
                (wiimote->classic.zr << 1) |
                (wiimote->classic.l << 2) | 
                (wiimote->classic.zl << 3);

        if (wiimote->extension == EXT_CLASSIC) {
            buf[5] = wiimote->classic.lx;
            buf[6] = wiimote->classic.ly;
        } else {
            buf[5] = wiimote->nunchuk.x;
            buf[6] = wiimote->nunchuk.y;
        }

        buf[7] = wiimote->classic.rx;
        buf[8] = wiimote->classic.ry;

        buf[9] = wiimote->accel_x & 0xFF;
        buf[10] = wiimote->accel_y & 0xFF;
        buf[11] = wiimote->accel_z & 0xFF;
        buf[12] = ((wiimote->accel_x & 0x300) >> 8) | 
                ((wiimote->accel_y & 0x300) >> 6) | 
                ((wiimote->accel_z & 0x300) >> 4);

        buf[13] = wiimote->nunchuk.accel_x & 0xFF;
        buf[14] = wiimote->nunchuk.accel_y & 0xFF;
        buf[15] = wiimote->nunchuk.accel_z & 0xFF;
        buf[16] = ((wiimote->nunchuk.accel_x & 0x300) >> 8) | 
                ((wiimote->nunchuk.accel_y & 0x300) >> 6) | 
                ((wiimote->nunchuk.accel_z & 0x300) >> 4);
        
        buf[17] = wiimote->motion_plus.yaw_speed & 0xFF;
        buf[18] = wiimote->motion_plus.pitch_speed & 0xFF;
        buf[19] = wiimote->motion_plus.roll_speed & 0xFF;
        buf[20] = ((wiimote->motion_plus.yaw_speed & 0xF00) >> 8) |
                ((wiimote->motion_plus.pitch_speed & 0xF00) >> 4);
        buf[21] = (wiimote->motion_plus.roll_speed & 0xF00) >> 8;

        if (wiimote->ir_idle) memset(buf + 22, 0xFF, 8);
        else {
            buf[22] = (uint16_t)wiimote->ir_object[0].x & 0xFF;
            buf[23] = ((uint16_t)wiimote->ir_object[0].x & 0xFF00) >> 8;
            buf[24] = (uint16_t)wiimote->ir_object[0].y & 0xFF;
            buf[25] = ((uint16_t)wiimote->ir_object[0].y & 0xFF00) >> 8;
            buf[26] = (uint16_t)wiimote->ir_object[1].x & 0xFF;
            buf[27] = ((uint16_t)wiimote->ir_object[1].x & 0xFF00) >> 8;
            buf[28] = (uint16_t)wiimote->ir_object[1].y & 0xFF;
            buf[29] = ((uint16_t)wiimote->ir_object[1].y & 0xFF00) >> 8;
        }

        uint8_t checksum_buttons = 0, checksum_axes = 0, i;
        for (i = 0; i < 4; i++) checksum_buttons += buf[i + 1];
        checksum_buttons += 0x55;
        for (i = 0; i < 25; i++) checksum_axes += buf[i + 5];
        checksum_axes += 0x55;

        buf[30] = checksum_buttons;
        buf[31] = checksum_axes;
    } else {
        uint8_t i;
        for (i = 0; i < 32; i++) buf[i] = 0xFF;
    }

    spi_slave_queue_data(buf, 32);
}

void wiimote_spi_receive() {
    uint8_t recv_buf_len, i;
    uint8_t * recv_buf;
    
    recv_buf = spi_slave_get_data(&recv_buf_len);
    if (!recv_buf) return;

    for (i = 0; i < 4; i++) {
        uint8_t wiimote_num = recv_buf[i] & 0x07;
        uint8_t connection_allowed = (recv_buf[i] & 0x08) >> 3;
        if (wiimote_num > 0 && wiimote_num <= 4) {
            hid_controller_t * controller_main;
            hid_controller_t * controller_secondary;
            get_controllers_from_gamepad(wiimote_num, &controller_main, &controller_secondary, 1);    // Only return connected controllers

            if (controller_main && !connection_allowed) controller_disconnect(controller_main);
            if (controller_secondary && !connection_allowed) controller_disconnect(controller_secondary);

            wiimotes[wiimote_num - 1].player_num = (recv_buf[i] & 0x70) >> 4;
            wiimotes[wiimote_num - 1].rumble = (recv_buf[i] & 0x80) >> 7;
        }
    }
}