#include <inttypes.h>
#include <stdio.h>
#include "btstack.h"
#include "gamepad.h"
#include "hid_controller.h"
#include "hid_command.h"
#include "uart_controller.h"
#include "wiimote.h"
#include "driver/timer.h"
#include "timer.h"

gamepad_t gamepads[4];

static int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
    int32_t result = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if (result < out_min) return out_min;
    if (result > out_max) return out_max;
    return result;
}

static void gamepad_reset_angles(gamepad_t * gamepad) {
    gamepad->yaw = 0;
    gamepad->pitch = 0;
    gamepad->roll = 0;
}

static void gamepad_input_defaults(gamepad_t * gamepad) {
    memset(&gamepad->buttons, 0, sizeof(buttons_t));
    memset(&gamepad->buttons, 0, sizeof(axes_t));
    gamepad->axes.joy_rx = 0x7FF;
    gamepad->axes.joy_ry = 0x7FF;
    gamepad->axes.joy_lx = 0x7FF;
    gamepad->axes.joy_ly = 0x7FF;
}

static void gamepad_cal_defaults(gamepad_t * gamepad) {
    gamepad->cal.joy_rx_max = 0xFFF;
    gamepad->cal.joy_rx_center = 0x7FF;
    gamepad->cal.joy_rx_min = 0;
    gamepad->cal.joy_ry_max = 0xFFF;
    gamepad->cal.joy_ry_center = 0x7FF;
    gamepad->cal.joy_ry_min = 0;
    gamepad->cal.accel_rx_offset = 0;
    gamepad->cal.accel_ry_offset = 0;
    gamepad->cal.accel_rz_offset = 0;
    gamepad->cal.gyro_rx_offset = 0;
    gamepad->cal.gyro_ry_offset = 0;
    gamepad->cal.gyro_rz_offset = 0;
    gamepad->cal.joy_lx_max = 0xFFF;
    gamepad->cal.joy_lx_center = 0x7FF;
    gamepad->cal.joy_lx_min = 0;
    gamepad->cal.joy_ly_max = 0xFFF;
    gamepad->cal.joy_ly_center = 0x7FF;
    gamepad->cal.joy_ly_min = 0;
    gamepad->cal.accel_lx_offset = 0;
    gamepad->cal.accel_ly_offset = 0;
    gamepad->cal.accel_lz_offset = 0;
    gamepad->cal.gyro_lx_offset = 0;
    gamepad->cal.gyro_ly_offset = 0;
    gamepad->cal.gyro_lz_offset = 0;
}

static uint8_t gamepad_handle_button_event(button_event_t * event, uint8_t button_state) {
    switch (event->state) {
        case 0: 
            if (button_state) {
                event->press_timer = app_timer;
                event->state = 1;
            }
            return 0;
        case 1:
            if ((app_timer - event->press_timer) >= LONG_PRESS_THRESH) {
                event->triggered = 1;
                event->state = 2; // Button must be released before retriggering
                //event->state = 0;   // Retrigger if button is held down
            } else if (!button_state) {
                event->press_timer = app_timer - event->press_timer;
                event->release_timer = app_timer;
                event->state = 3;
            }
            return 0;
        case 2:
            if (!button_state) event->state = 0;    // Wait for button to be released
            return 0;
        case 3:
            event->press_timer -= (app_timer - event->release_timer);   // Decrement by amount of time since last loop
            event->release_timer = app_timer;
            if (event->press_timer <= 0) event->state = 0;
            return 1;
        default:
            return 0;
    }
}

static void gamepad_parse_joycon_input(gamepad_t * gamepad, hid_controller_t * hid_joycon_right, hid_controller_t * hid_joycon_left) {
    if (hid_joycon_right) {
        switch (hid_joycon_right->status_response[1]) {
            case 0x30:
                gamepad->buttons.a = (hid_joycon_right->status_response[4] & 0x08) >> 3;
                gamepad->buttons.b = (hid_joycon_right->status_response[4] & 0x04) >> 2;
                gamepad->buttons.x = (hid_joycon_right->status_response[4] & 0x02) >> 1;
                gamepad->buttons.y = hid_joycon_right->status_response[4] & 0x01;
                gamepad->buttons.r = (hid_joycon_right->status_response[4] & 0x40) >> 6;
                gamepad->buttons.zr = (hid_joycon_right->status_response[4] & 0x80) >> 7;
                gamepad->buttons.plus = (hid_joycon_right->status_response[5] & 0x02) >> 1;
                gamepad->buttons.home = gamepad_handle_button_event(&gamepad->mode_switch, (hid_joycon_right->status_response[5] & 0x10) >> 4);
                gamepad->buttons.rs = (hid_joycon_right->status_response[5] & 0x04) >> 2;
                gamepad->buttons.srr = (hid_joycon_right->status_response[4] & 0x10) >> 4;
                gamepad->buttons.slr = (hid_joycon_right->status_response[4] & 0x20) >> 5;
                gamepad->axes.joy_rx = hid_joycon_right->status_response[10] | ((hid_joycon_right->status_response[11] & 0x0F) << 8);
                gamepad->axes.joy_ry = (hid_joycon_right->status_response[11] >> 4) | (hid_joycon_right->status_response[12] << 4);
                gamepad->axes.accel_rx = ((hid_joycon_right->status_response[15] << 8) | hid_joycon_right->status_response[14]) - gamepad->cal.accel_rx_offset;
                gamepad->axes.accel_ry = ((hid_joycon_right->status_response[17] << 8) | hid_joycon_right->status_response[16]) - gamepad->cal.accel_ry_offset;
                gamepad->axes.accel_rz = -(((hid_joycon_right->status_response[19] << 8) | hid_joycon_right->status_response[18]) - gamepad->cal.accel_rz_offset);
                gamepad->axes.gyro_rx = ((hid_joycon_right->status_response[21] << 8) | hid_joycon_right->status_response[20]) - (gamepad->calibrating ? 0 : gamepad->cal.gyro_rx_offset);   // Provide raw values for calibration
                gamepad->axes.gyro_ry = ((hid_joycon_right->status_response[23] << 8) | hid_joycon_right->status_response[22]) - (gamepad->calibrating ? 0 : gamepad->cal.gyro_ry_offset);
                gamepad->axes.gyro_rz = ((hid_joycon_right->status_response[25] << 8) | hid_joycon_right->status_response[24]) - (gamepad->calibrating ? 0 : gamepad->cal.gyro_rz_offset);

                if (gamepad->axes.joy_rx <= gamepad->cal.joy_rx_center)
                    gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, gamepad->cal.joy_rx_min, gamepad->cal.joy_rx_center, 0, 2047);
                else gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, gamepad->cal.joy_rx_center + 1, gamepad->cal.joy_rx_max, 2048, 4095);
                
                if (gamepad->axes.joy_ry <= gamepad->cal.joy_ry_center)
                    gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, gamepad->cal.joy_ry_min, gamepad->cal.joy_ry_center, 0, 2047);
                else gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, gamepad->cal.joy_ry_center + 1, gamepad->cal.joy_ry_max, 2048, 4095);
                break;
            case 0x3F:
                break;
            default:
                break;
        }
    }
    if (hid_joycon_left) {
        switch (hid_joycon_left->status_response[1]) {
            case 0x30:
                gamepad->buttons.du = (hid_joycon_left->status_response[6] & 0x02) >> 1;
                gamepad->buttons.dd = hid_joycon_left->status_response[6] & 0x01;
                gamepad->buttons.dr = (hid_joycon_left->status_response[6] & 0x04) >> 2;
                gamepad->buttons.dl = (hid_joycon_left->status_response[6] & 0x08) >> 3;
                gamepad->buttons.l = (hid_joycon_left->status_response[6] & 0x40) >> 6;
                gamepad->buttons.zl = (hid_joycon_left->status_response[6] & 0x80) >> 7;
                gamepad->buttons.minus = hid_joycon_left->status_response[5] & 0x01;
                gamepad->buttons.capture = (hid_joycon_left->status_response[5] & 0x20) >> 5;
                gamepad->buttons.ls = (hid_joycon_left->status_response[5] & 0x08) >> 3;
                gamepad->buttons.srl = (hid_joycon_left->status_response[6] & 0x10) >> 4;
                gamepad->buttons.sll = (hid_joycon_left->status_response[6] & 0x20) >> 5;
                gamepad->axes.joy_lx = hid_joycon_left->status_response[7] | ((hid_joycon_left->status_response[8] & 0x0F) << 8);
                gamepad->axes.joy_ly = (hid_joycon_left->status_response[8] >> 4) | (hid_joycon_left->status_response[9] << 4);
                gamepad->axes.accel_lx = ((hid_joycon_left->status_response[15] << 8) | hid_joycon_left->status_response[14]) - gamepad->cal.accel_lx_offset;
                gamepad->axes.accel_ly = ((hid_joycon_left->status_response[17] << 8) | hid_joycon_left->status_response[16]) - gamepad->cal.accel_ly_offset;
                gamepad->axes.accel_lz = ((hid_joycon_left->status_response[19] << 8) | hid_joycon_left->status_response[18]) - gamepad->cal.accel_lz_offset;
                gamepad->axes.gyro_lx = ((hid_joycon_left->status_response[21] << 8) | hid_joycon_left->status_response[20]) - gamepad->cal.gyro_lx_offset;
                gamepad->axes.gyro_ly = ((hid_joycon_left->status_response[23] << 8) | hid_joycon_left->status_response[22]) - gamepad->cal.gyro_ly_offset;
                gamepad->axes.gyro_lz = ((hid_joycon_left->status_response[25] << 8) | hid_joycon_left->status_response[24]) - gamepad->cal.gyro_lz_offset;

                if (gamepad->axes.joy_lx <= gamepad->cal.joy_lx_center)
                    gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_min, gamepad->cal.joy_lx_center, 0, 2047);
                else gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_center + 1, gamepad->cal.joy_lx_max, 2048, 4095);
                
                if (gamepad->axes.joy_ly <= gamepad->cal.joy_ly_center)
                    gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_min, gamepad->cal.joy_ly_center, 0, 2047);
                else gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_center + 1, gamepad->cal.joy_ly_max, 2048, 4095);
                break;
            case 0x3F:
                break;
            default:
                break;
        }
    }
}

static void gamepad_parse_procon_input(gamepad_t * gamepad, hid_controller_t * controller) {
    switch (controller->status_response[1]) {
        case 0x30:
            gamepad->buttons.a = (controller->status_response[4] & 0x08) >> 3;
            gamepad->buttons.b = (controller->status_response[4] & 0x04) >> 2;
            gamepad->buttons.x = (controller->status_response[4] & 0x02) >> 1;
            gamepad->buttons.y = controller->status_response[4] & 0x01;
            gamepad->buttons.du = (controller->status_response[6] & 0x02) >> 1;
            gamepad->buttons.dd = controller->status_response[6] & 0x01;
            gamepad->buttons.dr = (controller->status_response[6] & 0x04) >> 2;
            gamepad->buttons.dl = (controller->status_response[6] & 0x08) >> 3;
            gamepad->buttons.r = (controller->status_response[4] & 0x40) >> 6;
            gamepad->buttons.l = (controller->status_response[6] & 0x40) >> 6;
            gamepad->buttons.zr = (controller->status_response[4] & 0x80) >> 7;
            gamepad->buttons.zl = (controller->status_response[6] & 0x80) >> 7;
            gamepad->buttons.plus = (controller->status_response[5] & 0x02) >> 1;
            gamepad->buttons.minus = controller->status_response[5] & 0x01;
            gamepad->buttons.home = gamepad_handle_button_event(&gamepad->mode_switch, (controller->status_response[5] & 0x10) >> 4);
            gamepad->buttons.capture = (controller->status_response[5] & 0x20) >> 5;
            gamepad->buttons.rs = (controller->status_response[5] & 0x04) >> 2;
            gamepad->buttons.ls = (controller->status_response[5] & 0x08) >> 3;
            gamepad->axes.joy_rx = controller->status_response[10] | ((controller->status_response[11] & 0x0F) << 8);
            gamepad->axes.joy_ry = (controller->status_response[11] >> 4) | (controller->status_response[12] << 4);
            gamepad->axes.joy_lx = controller->status_response[7] | ((controller->status_response[8] & 0x0F) << 8);
            gamepad->axes.joy_ly = (controller->status_response[8] >> 4) | (controller->status_response[9] << 4);
            gamepad->axes.accel_ry = -(((controller->status_response[15] << 8) | controller->status_response[14]) - gamepad->cal.accel_rx_offset);
            gamepad->axes.accel_rx = -(((controller->status_response[17] << 8) | controller->status_response[16]) - gamepad->cal.accel_ry_offset);
            gamepad->axes.accel_rz = ((controller->status_response[19] << 8) | controller->status_response[18]) - gamepad->cal.accel_rz_offset;
            gamepad->axes.gyro_rx = ((controller->status_response[21] << 8) | controller->status_response[20]) - (gamepad->calibrating ? 0 : gamepad->cal.gyro_rx_offset);   // Provide raw values for calibration
            gamepad->axes.gyro_ry = ((controller->status_response[23] << 8) | controller->status_response[22]) - (gamepad->calibrating ? 0 : gamepad->cal.gyro_ry_offset);
            gamepad->axes.gyro_rz = ((controller->status_response[25] << 8) | controller->status_response[24]) - (gamepad->calibrating ? 0 : gamepad->cal.gyro_rz_offset);

            if (gamepad->axes.joy_rx <= gamepad->cal.joy_rx_center)
                gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, gamepad->cal.joy_rx_min, gamepad->cal.joy_rx_center, 0, 2047);
            else gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, gamepad->cal.joy_rx_center + 1, gamepad->cal.joy_rx_max, 2048, 4095);
            
            if (gamepad->axes.joy_ry <= gamepad->cal.joy_ry_center)
                gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, gamepad->cal.joy_ry_min, gamepad->cal.joy_ry_center, 0, 2047);
            else gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, gamepad->cal.joy_ry_center + 1, gamepad->cal.joy_ry_max, 2048, 4095);

            if (gamepad->axes.joy_lx <= gamepad->cal.joy_lx_center)
                gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_min, gamepad->cal.joy_lx_center, 0, 2047);
            else gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_center + 1, gamepad->cal.joy_lx_max, 2048, 4095);
            
            if (gamepad->axes.joy_ly <= gamepad->cal.joy_ly_center)
                gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_min, gamepad->cal.joy_ly_center, 0, 2047);
            else gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_center + 1, gamepad->cal.joy_ly_max, 2048, 4095);
            break;
        case 0x3F:
            break;
        default:
            break;
    }
}

static void gamepad_parse_extension_input(gamepad_t * gamepad, hid_controller_t * controller) {
    if (controller->wmp_active) {
        // Get WMP gyro data
        if ((controller->status_response[22] & 0x03) == 0x02) {
            gamepad->axes.gyro_rx = ((((controller->status_response[21] & 0xFC) << 6) | controller->status_response[18]) << 2) - 0x8000 - (gamepad->calibrating ? 0 : gamepad->cal.gyro_rx_offset);
            gamepad->axes.gyro_ry = ((((controller->status_response[22] & 0xFC) << 6) | controller->status_response[19]) << 2) - 0x8000 - (gamepad->calibrating ? 0 : gamepad->cal.gyro_ry_offset);
            gamepad->axes.gyro_rz = ((((controller->status_response[20] & 0xFC) << 6) | controller->status_response[17]) << 2) - 0x8000 - (gamepad->calibrating ? 0 : gamepad->cal.gyro_rz_offset);
        }
        // Get WMP passthrough data
        else if ((controller->status_response[22] & 0x03) == 0x00) {
            // TODO: Parse passthrough data
        }
    }
    else if (controller->wmp_type == WMP_NOT_SUPPORTED) {
        switch (controller->extension_type) {
            case EXT_NUNCHUK:
                gamepad->buttons.c = (~controller->status_response[22] & 0x02) >> 1;
                gamepad->buttons.z = ~controller->status_response[22] & 0x01;
                gamepad->axes.joy_lx = controller->status_response[17];
                gamepad->axes.joy_ly = controller->status_response[18];
                gamepad->axes.accel_lx = (int16_t)(((controller->status_response[19] << 2) | ((controller->status_response[22] & 0x0C) >> 2)) * 64) - 0x8000 - gamepad->cal.accel_lx_offset;
                gamepad->axes.accel_ly = (int16_t)(((controller->status_response[20] << 2) | ((controller->status_response[22] & 0x30) >> 4)) * 64) - 0x8000 - gamepad->cal.accel_ly_offset;
                gamepad->axes.accel_lz = (int16_t)(((controller->status_response[21] << 2) | ((controller->status_response[22] & 0xC0) >> 6)) * 64) - 0x8000 - gamepad->cal.accel_lz_offset;
                
                if (gamepad->axes.joy_lx <= gamepad->cal.joy_lx_center)
                    gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_min, gamepad->cal.joy_lx_center, 0, 2047);
                else gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_center + 1, gamepad->cal.joy_lx_max, 2048, 4095);
                
                if (gamepad->axes.joy_ly <= gamepad->cal.joy_ly_center)
                    gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_min, gamepad->cal.joy_ly_center, 0, 2047);
                else gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_center + 1, gamepad->cal.joy_ly_max, 2048, 4095);
                break;
            case EXT_CLASSIC:
                gamepad->buttons.a = (~controller->status_response[22] & 0x10) >> 4;
                gamepad->buttons.b = (~controller->status_response[22] & 0x40) >> 6;
                gamepad->buttons.x = (~controller->status_response[22] & 0x08) >> 3;
                gamepad->buttons.y = (~controller->status_response[22] & 0x20) >> 5;
                gamepad->buttons.du = ~controller->status_response[22] & 0x01;
                gamepad->buttons.dd = (~controller->status_response[21] & 0x40) >> 6;
                gamepad->buttons.dr = (~controller->status_response[21] & 0x80) >> 7;
                gamepad->buttons.dl = (~controller->status_response[22] & 0x02) >> 1;
                gamepad->buttons.r = (~controller->status_response[21] & 0x02) >> 1;
                gamepad->buttons.l = (~controller->status_response[21] & 0x20) >> 5;
                gamepad->buttons.zr = (~controller->status_response[22] & 0x04) >> 2;
                gamepad->buttons.zl = (~controller->status_response[22] & 0x80) >> 7;
                gamepad->buttons.plus = (~controller->status_response[21] & 0x04) >> 2;
                gamepad->buttons.minus = (~controller->status_response[21] & 0x10) >> 4;
                gamepad->buttons.home = (~controller->status_response[21] & 0x08) >> 3;
                gamepad->axes.joy_rx = (((controller->status_response[17] & 0xC0) >> 3) | ((controller->status_response[18] & 0xC0) >> 5) | ((controller->status_response[19] & 0x80) >> 7)) << 3;
                gamepad->axes.joy_ry = (controller->status_response[19] & 0x1F) << 3;
                gamepad->axes.joy_lx = (controller->status_response[17] & 0x3F) << 2;
                gamepad->axes.joy_ly = (controller->status_response[18] & 0x3F) << 2;

                if (gamepad->axes.joy_rx <= gamepad->cal.joy_rx_center)
                    gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, gamepad->cal.joy_rx_min, gamepad->cal.joy_rx_center, 0, 2047);
                else gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, gamepad->cal.joy_rx_center + 1, gamepad->cal.joy_rx_max, 2048, 4095);
                
                if (gamepad->axes.joy_ry <= gamepad->cal.joy_ry_center)
                    gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, gamepad->cal.joy_ry_min, gamepad->cal.joy_ry_center, 0, 2047);
                else gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, gamepad->cal.joy_ry_center + 1, gamepad->cal.joy_ry_max, 2048, 4095);

                if (gamepad->axes.joy_lx <= gamepad->cal.joy_lx_center)
                    gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_min, gamepad->cal.joy_lx_center, 0, 2047);
                else gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, gamepad->cal.joy_lx_center + 1, gamepad->cal.joy_lx_max, 2048, 4095);
                
                if (gamepad->axes.joy_ly <= gamepad->cal.joy_ly_center)
                    gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_min, gamepad->cal.joy_ly_center, 0, 2047);
                else gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, gamepad->cal.joy_ly_center + 1, gamepad->cal.joy_ly_max, 2048, 4095);
                break;
            default:
                break;
        }
    }
}

static void gamepad_parse_wiimote_input(gamepad_t * gamepad, hid_controller_t * controller) {
    gamepad->buttons.a = (controller->status_response[3] & 0x08) >> 3;
    gamepad->buttons.b = (controller->status_response[3] & 0x04) >> 2;
    gamepad->buttons.one = (controller->status_response[3] & 0x02) >> 1;
    gamepad->buttons.two = controller->status_response[3] & 0x01;
    gamepad->buttons.du = (controller->status_response[2] & 0x08) >> 3;
    gamepad->buttons.dd = (controller->status_response[2] & 0x04) >> 2;
    gamepad->buttons.dr = (controller->status_response[2] & 0x02) >> 1;
    gamepad->buttons.dl = controller->status_response[2] & 0x01;
    gamepad->buttons.home = (controller->status_response[3] & 0x80) >> 7;

    switch (controller->status_response[1]) {
        case 0x37:  // Buttons, accel, IR10, EXT6
            // TODO: Read IR data
            
            gamepad->axes.accel_rx = (int16_t)(((controller->status_response[4] << 2) | ((controller->status_response[2] & 0x60) >> 5)) * 64) - 0x8000 - gamepad->cal.accel_rx_offset;
            gamepad->axes.accel_ry = (int16_t)(((controller->status_response[5] << 2) | ((controller->status_response[3] & 0x20) >> 4)) * 64) - 0x8000 - gamepad->cal.accel_ry_offset;
            gamepad->axes.accel_rz = (int16_t)(((controller->status_response[6] << 2) | ((controller->status_response[3] & 0x40) >> 5)) * 64) - 0x8000 - gamepad->cal.accel_rz_offset;
            
            gamepad_parse_extension_input(gamepad, controller);
            break;
        case 0x33:  // Buttons, accel, IR12
            // TODO: Read IR data
            
            gamepad->axes.accel_rx = (int16_t)(((controller->status_response[4] << 2) | ((controller->status_response[2] & 0x60) >> 5)) * 64) - 0x8000 - gamepad->cal.accel_rx_offset;
            gamepad->axes.accel_ry = (int16_t)(((controller->status_response[5] << 2) | ((controller->status_response[3] & 0x20) >> 4)) * 64) - 0x8000 - gamepad->cal.accel_ry_offset;
            gamepad->axes.accel_rz = (int16_t)(((controller->status_response[6] << 2) | ((controller->status_response[3] & 0x40) >> 5)) * 64) - 0x8000 - gamepad->cal.accel_rz_offset;
            break;
        default:
            break;
    }

    if (controller->extension_type != EXT_CLASSIC) {
        gamepad->buttons.plus = gamepad_handle_button_event(&gamepad->gyro_center, (controller->status_response[2] & 0x10) >> 4);
        gamepad->buttons.minus = gamepad_handle_button_event(&gamepad->gyro_enable, (controller->status_response[3] & 0x10) >> 4);
        gamepad->buttons.home = gamepad_handle_button_event(&gamepad->gyro_calibrate, (controller->status_response[3] & 0x80) >> 7);
    }
}

static void gamepad_parse_wiiu_pro_input(gamepad_t * gamepad, hid_controller_t * controller) {
    gamepad->buttons.a = (~controller->status_response[11] & 0x10) >> 4;
    gamepad->buttons.b = (~controller->status_response[11] & 0x40) >> 6;
    gamepad->buttons.x = (~controller->status_response[11] & 0x08) >> 3;
    gamepad->buttons.y = (~controller->status_response[11] & 0x20) >> 5;
    gamepad->buttons.du = ~controller->status_response[11] & 0x01;
    gamepad->buttons.dd = (~controller->status_response[10] & 0x40) >> 6;
    gamepad->buttons.dr = (~controller->status_response[10] & 0x80) >> 7;
    gamepad->buttons.dl = (~controller->status_response[11] & 0x02) >> 1;
    gamepad->buttons.r = (~controller->status_response[10] & 0x02) >> 1;
    gamepad->buttons.l = (~controller->status_response[10] & 0x20) >> 5;
    gamepad->buttons.zr = (~controller->status_response[11] & 0x04) >> 2;
    gamepad->buttons.zl = (~controller->status_response[11] & 0x80) >> 7;
    gamepad->buttons.plus = (~controller->status_response[10] & 0x04) >> 2;
    gamepad->buttons.minus = (~controller->status_response[10] & 0x10) >> 4;
    gamepad->buttons.home = gamepad_handle_button_event(&gamepad->mode_switch, (~controller->status_response[10] & 0x08) >> 3);
    gamepad->buttons.rs = ~controller->status_response[12] & 0x01;
    gamepad->buttons.ls = (~controller->status_response[12] & 0x02) >> 1;
    gamepad->axes.joy_rx = controller->status_response[4] | ((controller->status_response[5] & 0x0F) << 8);
    gamepad->axes.joy_ry = controller->status_response[8] | ((controller->status_response[9] & 0x0F) << 8);
    gamepad->axes.joy_lx = controller->status_response[2] | ((controller->status_response[3] & 0x0F) << 8);
    gamepad->axes.joy_ly = controller->status_response[6] | ((controller->status_response[7] & 0x0F) << 8);
    
    gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, 800, 3295, 0, 4095);
    gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, 800, 3295, 0, 4095);
    gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, 800, 3295, 0, 4095);
    gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, 800, 3295, 0, 4095);
}

static void gamepad_parse_uart_input(gamepad_t * gamepad) {
    if (joycon_right.data_ready) {
        gamepad->buttons.a = (joycon_right.rx_buf[15] & 0x08) >> 3;
        gamepad->buttons.b = (joycon_right.rx_buf[15] & 0x04) >> 2;
        gamepad->buttons.x = (joycon_right.rx_buf[15] & 0x02) >> 1;
        gamepad->buttons.y = joycon_right.rx_buf[15] & 0x01;
        gamepad->buttons.r = (joycon_right.rx_buf[15] & 0x40) >> 6;
        gamepad->buttons.zr = (joycon_right.rx_buf[15] & 0x80) >> 7;
        gamepad->buttons.plus = (joycon_right.rx_buf[16] & 0x02) >> 1;
        gamepad->buttons.home = gamepad_handle_button_event(&gamepad->mode_switch, (joycon_right.rx_buf[16] & 0x10) >> 4);
        gamepad->buttons.rs = (joycon_right.rx_buf[16] & 0x04) >> 2;
        gamepad->buttons.srr = (joycon_right.rx_buf[15] & 0x10) >> 4;
        gamepad->buttons.slr = (joycon_right.rx_buf[15] & 0x20) >> 5;
        gamepad->axes.joy_rx = ((joycon_right.rx_buf[22] & 0x0F << 4) | (joycon_right.rx_buf[22] & 0xF0 >> 4)) << 4;
        gamepad->axes.joy_ry = joycon_right.rx_buf[21] << 4;
        gamepad->axes.gyro_rx = ((joycon_right.rx_buf[32] << 8) | joycon_right.rx_buf[31]) - joycon_right.gyro_x_offset;
        gamepad->axes.gyro_ry = ((joycon_right.rx_buf[34] << 8) | joycon_right.rx_buf[33]) - joycon_right.gyro_y_offset;
        gamepad->axes.gyro_rz = ((joycon_right.rx_buf[36] << 8) | joycon_right.rx_buf[35]) - joycon_right.gyro_z_offset;
        gamepad->axes.accel_rx = ((joycon_right.rx_buf[38] << 8) | joycon_right.rx_buf[37]) - joycon_right.accel_x_offset;
        gamepad->axes.accel_ry = ((joycon_right.rx_buf[40] << 8) | joycon_right.rx_buf[39]) - joycon_right.accel_y_offset;
        gamepad->axes.accel_rz = ((joycon_right.rx_buf[42] << 8) | joycon_right.rx_buf[41]) - joycon_right.accel_z_offset;

        if (gamepad->axes.joy_rx <= joycon_right.joy_x_center)
            gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, joycon_right.joy_x_min, joycon_right.joy_x_center, 0, 2047);
        else gamepad->axes.joy_rx = map(gamepad->axes.joy_rx, joycon_right.joy_x_center + 1, joycon_right.joy_x_max, 2048, 4095);
        
        if (gamepad->axes.joy_ry <= joycon_right.joy_y_center)
            gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, joycon_right.joy_y_min, joycon_right.joy_y_center, 0, 2047);
        else gamepad->axes.joy_ry = map(gamepad->axes.joy_ry, joycon_right.joy_y_center + 1, joycon_right.joy_y_max, 2048, 4095);
    }
    if (joycon_left.data_ready) {
        gamepad->buttons.du = (joycon_left.rx_buf[17] & 0x02) >> 1;
        gamepad->buttons.dd = joycon_left.rx_buf[17] & 0x01;
        gamepad->buttons.dr = (joycon_left.rx_buf[17] & 0x04) >> 2;
        gamepad->buttons.dl = (joycon_left.rx_buf[17] & 0x08) >> 3;
        gamepad->buttons.l = (joycon_left.rx_buf[17] & 0x40) >> 6;
        gamepad->buttons.zl = (joycon_left.rx_buf[17] & 0x80) >> 7;
        gamepad->buttons.minus = joycon_left.rx_buf[16] & 0x01;
        gamepad->buttons.capture = (joycon_left.rx_buf[16] & 0x20) >> 5;
        gamepad->buttons.ls = (joycon_left.rx_buf[16] & 0x08) >> 3;
        gamepad->buttons.srl = (joycon_left.rx_buf[17] & 0x10) >> 4;
        gamepad->buttons.sll = (joycon_left.rx_buf[17] & 0x20) >> 5;
        gamepad->axes.joy_lx = ((joycon_left.rx_buf[19] & 0x0F << 4) | (joycon_left.rx_buf[19] & 0xF0 >> 4)) << 4;
        gamepad->axes.joy_ly = joycon_left.rx_buf[20] << 4;
        gamepad->axes.gyro_lx = ((joycon_left.rx_buf[32] << 8) | joycon_left.rx_buf[31]) - joycon_left.gyro_x_offset;
        gamepad->axes.gyro_ly = ((joycon_left.rx_buf[34] << 8) | joycon_left.rx_buf[33]) - joycon_left.gyro_y_offset;
        gamepad->axes.gyro_lz = ((joycon_left.rx_buf[36] << 8) | joycon_left.rx_buf[35]) - joycon_left.gyro_z_offset;
        gamepad->axes.accel_lx = ((joycon_left.rx_buf[38] << 8) | joycon_left.rx_buf[37]) - joycon_left.accel_x_offset;
        gamepad->axes.accel_ly = ((joycon_left.rx_buf[40] << 8) | joycon_left.rx_buf[39]) - joycon_left.accel_y_offset;
        gamepad->axes.accel_lz = ((joycon_left.rx_buf[42] << 8) | joycon_left.rx_buf[41]) - joycon_left.accel_z_offset;

        if (gamepad->axes.joy_lx <= joycon_left.joy_x_center)
            gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, joycon_left.joy_x_min, joycon_left.joy_x_center, 0, 2047);
        else gamepad->axes.joy_lx = map(gamepad->axes.joy_lx, joycon_left.joy_x_center + 1, joycon_left.joy_x_max, 2048, 4095);
        
        if (gamepad->axes.joy_ly <= joycon_left.joy_y_center)
            gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, joycon_left.joy_y_min, joycon_left.joy_y_center, 0, 2047);
        else gamepad->axes.joy_ly = map(gamepad->axes.joy_ly, joycon_left.joy_y_center + 1, joycon_left.joy_y_max, 2048, 4095);
    }
}

// TODO: Calibrate accel as well so user calibration can be properly updated (for Switch controllers)
static uint8_t gamepad_gyro_calibrate(gamepad_t * gamepad) {
    if (gamepad->calibrating) {
        if ((app_timer - gamepad->calibration_timer >= 6500) || (gamepad->calibrate_num_samples == 255)) {
            gamepad->cal.gyro_rx_offset = gamepad->calibrate_gyro_x_sum / gamepad->calibrate_num_samples;
            gamepad->cal.gyro_ry_offset = gamepad->calibrate_gyro_y_sum / gamepad->calibrate_num_samples;
            gamepad->cal.gyro_rz_offset = gamepad->calibrate_gyro_z_sum / gamepad->calibrate_num_samples;
            gamepad->calibrating = 0;
            printf("Gyro X offset: %d\n", gamepad->cal.gyro_rx_offset);
            printf("Gyro Y offset: %d\n", gamepad->cal.gyro_ry_offset);
            printf("Gyro Z offset: %d\n", gamepad->cal.gyro_rz_offset);
            return 1;
        } else {
            if (app_timer - gamepad->calibration_timer >= 3000) { // Start calibration after 3s
                gamepad->calibrate_gyro_x_sum += gamepad->axes.gyro_rx;
                gamepad->calibrate_gyro_y_sum += gamepad->axes.gyro_ry;
                gamepad->calibrate_gyro_z_sum += gamepad->axes.gyro_rz;
                gamepad->calibrate_num_samples++;
            }
        }
    } else {
        gamepad->calibrating = 1; 
        gamepad->calibration_timer = app_timer;
        gamepad->calibrate_gyro_x_sum = 0;
        gamepad->calibrate_gyro_y_sum = 0;
        gamepad->calibrate_gyro_z_sum = 0;
        gamepad->calibrate_num_samples = 0;
        gamepad_reset_angles(gamepad);
        printf("Calibrating\n");
    }
    return 0;
}

// Handle centering and calibration (angle integration is performed in HID packet handler)
static void gamepad_gyro_handle(gamepad_t * gamepad, hid_controller_t * controller) {  
    if (gamepad->calibrating) {
        if (gamepad_gyro_calibrate(gamepad) && controller) {
            // Store calibration data in memory
            uint8_t accel_data[6];
            uint8_t gyro_data[6];
            switch (controller->type) {
                case CNT_JOYCON_R:
                case CNT_PROCON:
                    accel_data[0] = gamepad->cal.accel_rx_offset & 0xFF; 
                    accel_data[1] = (gamepad->cal.accel_rx_offset & 0xFF00) >> 8;
                    accel_data[2] = gamepad->cal.accel_ry_offset & 0xFF; 
                    accel_data[3] = (gamepad->cal.accel_ry_offset & 0xFF00) >> 8;
                    accel_data[4] = gamepad->cal.accel_rz_offset & 0xFF; 
                    accel_data[5] = (gamepad->cal.accel_rz_offset & 0xFF00) >> 8;
                    gyro_data[0] = gamepad->cal.gyro_rx_offset & 0xFF;
                    gyro_data[1] = (gamepad->cal.gyro_rx_offset & 0xFF00) >> 8;
                    gyro_data[2] = gamepad->cal.gyro_ry_offset & 0xFF; 
                    gyro_data[3] = (gamepad->cal.gyro_ry_offset & 0xFF00) >> 8;
                    gyro_data[4] = gamepad->cal.gyro_rz_offset & 0xFF; 
                    gyro_data[5] = (gamepad->cal.gyro_rz_offset & 0xFF00) >> 8;
                    hid_queue_command(controller, &cmd_joycon_write_cal_accel_user, accel_data, NULL, 1);
                    hid_queue_command(controller, &cmd_joycon_write_cal_gyro_user, gyro_data, NULL, 1);
                    hid_queue_command(controller, &cmd_joycon_write_magic_imu_user, NULL, NULL, 1);
                    break;
                case CNT_WIIMOTE:
                    accel_data[0] = ((gamepad->cal.accel_rx_offset + 0x8000) & 0xFF00) >> 8;
                    accel_data[1] = ((gamepad->cal.accel_ry_offset + 0x8000) & 0xFF00) >> 8;
                    accel_data[2] = ((gamepad->cal.accel_rz_offset + 0x8000) & 0xFF00) >> 8;
                    accel_data[3] = (((gamepad->cal.accel_rx_offset + 0x8000) & 0x00C0) >> 2) | ((gamepad->cal.accel_ry_offset & 0x00C0) >> 4) | ((gamepad->cal.accel_rz_offset & 0x00C0) >> 6);
                    gyro_data[0] = ((gamepad->cal.gyro_rz_offset + 0x8000) & 0xFF00) >> 8;
                    gyro_data[1] = (gamepad->cal.gyro_rz_offset + 0x8000) & 0xFF;
                    gyro_data[2] = ((gamepad->cal.gyro_rx_offset + 0x8000) & 0xFF00) >> 8;
                    gyro_data[3] = (gamepad->cal.gyro_rx_offset + 0x8000) & 0xFF; 
                    gyro_data[4] = ((gamepad->cal.gyro_ry_offset + 0x8000) & 0xFF00) >> 8;
                    gyro_data[5] = (gamepad->cal.gyro_ry_offset + 0x8000) & 0xFF; 
                    //hid_queue_command(controller, &cmd_wiimote_write_cal_accel, accel_data, NULL, 1); // Doesn't work?
                    hid_queue_command(controller, &cmd_wiimote_write_cal_wmp, gyro_data, NULL, 1);  // Only seems to work for newer Wii Remotes
                    break;
                default:
                    break;
            }
        }
    }
    else {
        if (gamepad->type == GAMEPAD_WIIMOTE) { // Wiimote uses long-press to center and long-press to calibrate
            if (gamepad->gyro_center.triggered) {
                gamepad_reset_angles(gamepad);
                if (controller) controller_rumble_pattern(controller, 70, 0, 1);
                gamepad->gyro_center.triggered = 0;
            }
            if (gamepad->gyro_calibrate.triggered) {
                gamepad_gyro_calibrate(gamepad);
                if (controller) controller_rumble_pattern(controller, 100, 500, 5);
                gamepad->gyro_calibrate.triggered = 0;
            }
            // Add option to toggel gyro/IR camera with gyro_enable
        } else {  // Other controllers use short-press to center and long-press to calibrate
            if (gamepad->buttons.r) gamepad_reset_angles(gamepad);
            else gamepad->calibration_timer = app_timer;

            if (app_timer - gamepad->calibration_timer >= LONG_PRESS_THRESH) {
                gamepad_gyro_calibrate(gamepad);
                if (controller) controller_rumble_pattern(controller, 100, 500, 5);
            }
        }
    }
}

void gamepad_reset(gamepad_t * gamepad) {
	memset(gamepad, 0, sizeof(gamepad_t));
    gamepad->type = GAMEPAD_NONE;
    gamepad_input_defaults(gamepad);
    gamepad_cal_defaults(gamepad);
    gamepad_reset_angles(gamepad);
    gamepad->imu_timer = 0;
}

void init_gamepads() {
    gamepad_reset(&gamepads[0]);
    gamepad_reset(&gamepads[1]);
    gamepad_reset(&gamepads[2]);
    gamepad_reset(&gamepads[3]);
}

void gamepad_set_type(uint8_t gamepad_num) {
    if (gamepad_num > 0 && gamepad_num <= 4) {
        gamepad_t * gamepad = &gamepads[gamepad_num - 1];
        hid_controller_t * controller_main;
        hid_controller_t * controller_secondary;
        get_controllers_from_gamepad(gamepad_num, &controller_main, &controller_secondary, 0);
        if (!controller_main && !controller_secondary) return;

        if (controller_main) {
            switch (controller_main->type) {
                case CNT_JOYCON_R:
                    if (controller_secondary) gamepad->type = GAMEPAD_JOYCON_DUAL;
                    else gamepad->type = GAMEPAD_JOYCON_R_VERT;
                    break;
                case CNT_PROCON:
                    gamepad->type = GAMEPAD_PROCON;
                    break;
                case CNT_WIIMOTE:
                    gamepad->type = GAMEPAD_WIIMOTE;
                    break;
                case CNT_WIIU_PRO:
                    gamepad->type = GAMEPAD_WIIU_PRO;
                    break;
                default:
                    break;
            }
        } else gamepad->type = GAMEPAD_JOYCON_L_VERT;
    }
}

void gamepad_set_extension(uint8_t gamepad_num) {
    if (gamepad_num > 0 && gamepad_num <= 4) {
        gamepad_t * gamepad = &gamepads[gamepad_num - 1];
        
        // Set initial Wiimote extension
        switch (gamepad->type) {
            case GAMEPAD_JOYCON_DUAL:
            case GAMEPAD_JOYCON_WIRED:
            case GAMEPAD_JOYCON_L_VERT:
                wiimote_set_extension(gamepad_num, EXT_NUNCHUK);
                break;
            case GAMEPAD_WIIMOTE:
            case GAMEPAD_WIIU_PRO:
            case GAMEPAD_PROCON:
            case GAMEPAD_JOYCON_R_VERT:
                wiimote_set_extension(gamepad_num, EXT_NONE);
                break;
            default:
                break;
        }
    }
}

// Use gamepad IMU values to integrate yaw, pitch, roll (called from HID packet handler)
void gamepad_get_angles_from_controller(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    
    if (!gamepad->calibrating) {
        uint64_t cur_time;
        double delta_t;
        double pitch_speed;
        double yaw_speed;
        int16_t gyro_y1, gyro_y2, gyro_y3, gyro_z1, gyro_z2, gyro_z3;
        timer_get_counter_value(TIMER_GROUP_1, TIMER_1, &cur_time);
        delta_t = (double)(cur_time - gamepad->imu_timer) / 1000000 - 0.01;  // Time between last calculation and first new reading (ideally 5ms, assuming Joy-Con pushes packets every 15ms)
        if (delta_t < 0) delta_t = 0;
        //printf("Period: %llu\n", cur_time - gamepad->imu_timer);

        switch (controller->type) {
            case CNT_JOYCON_R:
            case CNT_PROCON:
                if ((controller->status_response[1] == 0x30) && (cur_time - gamepad->imu_timer >= 15000)) {
                    // Pro controller axes are inverted
                    gyro_y1 = (int16_t)(((controller->status_response[23] << 8) | controller->status_response[22]) - gamepad->cal.gyro_ry_offset) * ((controller->type == CNT_PROCON) ? -1 : 1);
                    gyro_y2 = (int16_t)(((controller->status_response[35] << 8) | controller->status_response[34]) - gamepad->cal.gyro_ry_offset) * ((controller->type == CNT_PROCON) ? -1 : 1);
                    gyro_y3 = (int16_t)(((controller->status_response[47] << 8) | controller->status_response[46]) - gamepad->cal.gyro_ry_offset) * ((controller->type == CNT_PROCON) ? -1 : 1);
                    gyro_z1 = (int16_t)(((controller->status_response[25] << 8) | controller->status_response[24]) - gamepad->cal.gyro_rz_offset) * ((controller->type == CNT_PROCON) ? -1 : 1);
                    gyro_z2 = (int16_t)(((controller->status_response[37] << 8) | controller->status_response[36]) - gamepad->cal.gyro_rz_offset) * ((controller->type == CNT_PROCON) ? -1 : 1);
                    gyro_z3 = (int16_t)(((controller->status_response[49] << 8) | controller->status_response[48]) - gamepad->cal.gyro_rz_offset) * ((controller->type == CNT_PROCON) ? -1 : 1);

                    pitch_speed = gyro_y3 * 4588 / 0xFFFF;
                    yaw_speed = gyro_z3 * 4588 / 0xFFFF;
                    gamepad->pitch += pitch_speed * delta_t;
                    gamepad->yaw += yaw_speed * delta_t;

                    pitch_speed = gyro_y2 * 4588 / 0xFFFF;
                    yaw_speed = gyro_z2 * 4588 / 0xFFFF;
                    gamepad->pitch += pitch_speed * 0.005;  // Fixed time of 5ms between consecutive readings 
                    gamepad->yaw += yaw_speed * 0.005;

                    pitch_speed = gyro_y1 * 4588 / 0xFFFF;
                    yaw_speed = gyro_z1 * 4588 / 0xFFFF;
                    gamepad->pitch += pitch_speed * 0.005;
                    gamepad->yaw += yaw_speed * 0.005;

                    gamepad->imu_timer = cur_time;
                    //printf("Pitch: %f    Yaw: %f\n", gamepad->pitch, gamepad->yaw);
                }
                break;
            case CNT_WIIMOTE:
                if ((controller->status_response[1] == 0x37) && controller->wmp_active && ((controller->status_response[22] & 0x03) == 0x02)) {
                    gyro_y1 = ((((controller->status_response[22] & 0xFC) << 6) | controller->status_response[19]) << 2) - 0x8000 - gamepad->cal.gyro_ry_offset;
                    gyro_z1 = ((((controller->status_response[20] & 0xFC) << 6) | controller->status_response[17]) << 2) - 0x8000 - gamepad->cal.gyro_rz_offset;
                
                    double pitch_speed = gyro_y1 * 1440 / 0xFFFF;   // These constants only seem to work correctly for TR Wiimotes
                    double yaw_speed = gyro_z1 * 1440 / 0xFFFF;
                    gamepad->pitch += pitch_speed * delta_t;
                    gamepad->yaw += yaw_speed * delta_t;

                    gamepad->imu_timer = cur_time;
                    //printf("Pitch: %f    Yaw: %f\n", gamepad->pitch, gamepad->yaw);
                }
                break;
            default:
                break;
        }
    }
}

void gamepad_handle(uint8_t gamepad_num) {
    if (gamepad_num > 0 && gamepad_num <= 4) {
        gamepad_t * gamepad = &gamepads[gamepad_num - 1];
        hid_controller_t * controller_main;
        hid_controller_t * controller_secondary;
        get_controllers_from_gamepad(gamepad_num, &controller_main, &controller_secondary, 1);    // Only return connected controllers

        uint8_t using_wired_joycon = (gamepad_num == UART_GAMEPAD_NUM) && (joycon_right.data_ready || joycon_left.data_ready);
        if (!controller_main && !controller_secondary && !using_wired_joycon) {
            wiimotes[gamepad_num - 1].active = 0;
            return;
        }
        wiimotes[gamepad_num - 1].active = 1;

        // TODO: Allow wired Joy-Con to work alongside player 1 wireless controller
        gamepad_input_defaults(gamepad);
        if (using_wired_joycon) {
            gamepad->type = GAMEPAD_JOYCON_WIRED;
            gamepad_parse_uart_input(gamepad);
        } else {
            gamepad_set_type(gamepad_num);  // This has to be set every loop to account for wired Joy-Con disconnecting
            switch (gamepad->type) {
                case GAMEPAD_JOYCON_R_VERT:
                case GAMEPAD_JOYCON_R_SIDE:
                case GAMEPAD_JOYCON_DUAL:
                    gamepad_parse_joycon_input(gamepad, controller_main, controller_secondary);
                    if (wiimotes[gamepad_num - 1].extension != EXT_CLASSIC) gamepad_gyro_handle(gamepad, controller_main);
                    break;
                case GAMEPAD_JOYCON_L_VERT:
                case GAMEPAD_JOYCON_L_SIDE:
                    gamepad_parse_joycon_input(gamepad, controller_main, controller_secondary);
                    break;
                case GAMEPAD_PROCON:
                    gamepad_parse_procon_input(gamepad, controller_main);
                    if (wiimotes[gamepad_num - 1].extension != EXT_CLASSIC) gamepad_gyro_handle(gamepad, controller_main);
                    break;
                case GAMEPAD_WIIMOTE:
                    gamepad_parse_wiimote_input(gamepad, controller_main);
                    if (controller_main->wmp_active) gamepad_gyro_handle(gamepad, controller_main);
                    // Enable gyro pointer by long pressing minus
                    break;
                case GAMEPAD_WIIU_PRO:
                    gamepad_parse_wiiu_pro_input(gamepad, controller_main);
                    break;
                default:
                    break;
            }
        }

        if (gamepad->type == GAMEPAD_WIIMOTE) {
            if (controller_main) wiimote_set_extension(gamepad_num, controller_main->extension_type);
        } else {
            if (gamepad->mode_switch.triggered) {
                wiimote_change_extension(gamepad_num);  // Cycle through virtual extensions
                if (controller_main) controller_rumble_pattern(controller_main, 100, 70, 2);
                if (controller_secondary) controller_rumble_pattern(controller_secondary, 100, 70, 2);
                gamepad->mode_switch.triggered = 0;
            }
        }

        
        wiimote_handle(gamepad_num);
        
        // Set controller LEDs and rumble based on response from PIC
        if (controller_main) {
            if ((controller_main->rumble != wiimotes[gamepad_num - 1].rumble) && !controller_main->rumble_pattern_repetitions)
                controller_rumble(controller_main, wiimotes[gamepad_num - 1].rumble);
            if (wiimotes[gamepad_num - 1].player_num) 
                controller_set_leds(controller_main, wiimotes[gamepad_num - 1].player_num);
        }
        if (controller_secondary) {
            if ((controller_secondary->rumble != wiimotes[gamepad_num - 1].rumble) && !controller_secondary->rumble_pattern_repetitions) 
                controller_rumble(controller_secondary, wiimotes[gamepad_num - 1].rumble);
            if (wiimotes[gamepad_num - 1].player_num) 
                controller_set_leds(controller_secondary, wiimotes[gamepad_num - 1].player_num);
        }

        // TO DO
        // Allow both Bluetooth and wired controllers to control player 1 at the same time
        // If Joy-Con SL+SR is held, change that gamepad type to single Joy-Con and disconnect the other Joy-Con that was in dual mode with it
    }
}

// Set calibration values specific to Nunchuk
void gamepad_set_cal_nunchuk(hid_controller_t * controller) {
    gamepad_set_cal_accel_l(controller);
    gamepad_set_cal_joy_l(controller);
}

// Set calibration values specific to Classic controller
void gamepad_set_cal_classic(hid_controller_t * controller) {
    gamepad_set_cal_joy_r(controller);
    gamepad_set_cal_joy_l(controller);
}

// Set calibration for controllers with IMUs (not separate accel and gyro)
void gamepad_set_cal_imu(hid_controller_t * controller) {
    gamepad_set_cal_accel_r(controller);
    gamepad_set_cal_accel_l(controller);
    gamepad_set_cal_gyro_r(controller);
    gamepad_set_cal_gyro_l(controller);
}

void gamepad_set_cal_accel_r(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    switch (controller->type) {
        case CNT_JOYCON_R:
        case CNT_PROCON:
            switch (controller->cal_accel_r_type) {
                case 0:
                    if ((controller->command_response[21] == 0xB2) && (controller->command_response[22] == 0xA1)) { // Magic identifier for user calibration
                        gamepad->cal.accel_rx_offset = (controller->command_response[24] << 8) | controller->command_response[23];
                        gamepad->cal.accel_ry_offset = (controller->command_response[26] << 8) | controller->command_response[25];
                        gamepad->cal.accel_rz_offset = (controller->command_response[28] << 8) | controller->command_response[27];
                    } else {
                        controller->cal_accel_r_type = 1;
                        hid_queue_command(controller, &cmd_joycon_read_cal_imu_factory, NULL, &gamepad_set_cal_imu, 1);
                    }
                    break;
                case 1:
                    gamepad->cal.accel_rx_offset = (controller->command_response[22] << 8) | controller->command_response[21];
                    gamepad->cal.accel_ry_offset = (controller->command_response[24] << 8) | controller->command_response[23];
                    gamepad->cal.accel_rz_offset = (controller->command_response[26] << 8) | controller->command_response[25];
                    break;
                default:
                    break;
            }
            printf("Right accel X offset: %d\n", gamepad->cal.accel_rx_offset);
            printf("Right accel Y offset: %d\n", gamepad->cal.accel_ry_offset);
            printf("Right accel Z offset: %d\n", gamepad->cal.accel_rz_offset);
            break;
        case CNT_WIIMOTE:
            gamepad->cal.accel_rx_offset = (((controller->command_response[7] << 2) | ((controller->command_response[10] & 0x30) >> 2)) << 6) - 0x8000;
            gamepad->cal.accel_ry_offset = (((controller->command_response[8] << 2) | ((controller->command_response[10] & 0x0C) >> 2)) << 6) - 0x8000;
            gamepad->cal.accel_rz_offset = (((controller->command_response[9] << 2) | (controller->command_response[10] & 0x03)) << 6) - 0x8000;
            printf("Right accel X offset: %d\n", gamepad->cal.accel_rx_offset);
            printf("Right accel Y offset: %d\n", gamepad->cal.accel_ry_offset);
            printf("Right accel Z offset: %d\n", gamepad->cal.accel_rz_offset);
            break;
        default:
            break;
    }
}

void gamepad_set_cal_accel_l(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    uint8_t i, checksum = 0;
    switch (controller->type) {
        case CNT_JOYCON_L:
            switch (controller->cal_accel_l_type) {
                case 0:
                    if ((controller->command_response[21] == 0xB2) && (controller->command_response[22] == 0xA1)) { // Magic identifier for user calibration
                        gamepad->cal.accel_lx_offset = (controller->command_response[24] << 8) | controller->command_response[23];
                        gamepad->cal.accel_ly_offset = (controller->command_response[26] << 8) | controller->command_response[25];
                        gamepad->cal.accel_lz_offset = (controller->command_response[28] << 8) | controller->command_response[27];
                    } else {
                        controller->cal_accel_l_type = 1;
                        hid_queue_command(controller, &cmd_joycon_read_cal_imu_factory, NULL, &gamepad_set_cal_imu, 1);
                    }
                    break;
                case 1:
                    gamepad->cal.accel_lx_offset = (controller->command_response[22] << 8) | controller->command_response[21];
                    gamepad->cal.accel_ly_offset = (controller->command_response[24] << 8) | controller->command_response[23];
                    gamepad->cal.accel_lz_offset = (controller->command_response[26] << 8) | controller->command_response[25];
                    break;
                default:
                    break;
            }
            printf("Left accel X offset: %d\n", gamepad->cal.accel_lx_offset);
            printf("Left accel Y offset: %d\n", gamepad->cal.accel_ly_offset);
            printf("Left accel Z offset: %d\n", gamepad->cal.accel_lz_offset);
            break;
        case CNT_WIIMOTE:
            for(i = 0; i < 14; i++) checksum += controller->command_response[7 + i];
            checksum += 0x55;
            if (checksum == controller->command_response[21]) {
                gamepad->cal.accel_lx_offset = (((controller->command_response[7] << 2) | ((controller->command_response[10] & 0x30) >> 2)) << 6) - 0x8000;
                gamepad->cal.accel_ly_offset = (((controller->command_response[8] << 2) | ((controller->command_response[10] & 0x0C) >> 2)) << 6) - 0x8000;
                gamepad->cal.accel_lz_offset = (((controller->command_response[9] << 2) | (controller->command_response[10] & 0x03)) << 6) - 0x8000;
            } else {
                gamepad->cal.accel_lx_offset = 0;
                gamepad->cal.accel_ly_offset = 0;
                gamepad->cal.accel_lz_offset = 0;
            }
            printf("Left accel X offset: %d\n", gamepad->cal.accel_lx_offset);
            printf("Left accel Y offset: %d\n", gamepad->cal.accel_ly_offset);
            printf("Left accel Z offset: %d\n", gamepad->cal.accel_lz_offset);
            break;
        default:
            break;
    }
}

void gamepad_set_cal_gyro_r(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    switch (controller->type) {
        case CNT_JOYCON_R:
        case CNT_PROCON:
            switch (controller->cal_gyro_r_type) {
                case 0:
                    if ((controller->command_response[21] == 0xB2) && (controller->command_response[22] == 0xA1)) { // Magic identifier for user calibration
                        gamepad->cal.gyro_rx_offset = (controller->command_response[36] << 8) | controller->command_response[35];
                        gamepad->cal.gyro_ry_offset = (controller->command_response[38] << 8) | controller->command_response[37];
                        gamepad->cal.gyro_rz_offset = (controller->command_response[40] << 8) | controller->command_response[39];
                    } else {
                        controller->cal_gyro_r_type = 1;
                        hid_queue_command(controller, &cmd_joycon_read_cal_imu_factory, NULL, &gamepad_set_cal_imu, 1);
                    }
                    break;
                case 1:
                    gamepad->cal.gyro_rx_offset = (controller->command_response[34] << 8) | controller->command_response[33];
                    gamepad->cal.gyro_ry_offset = (controller->command_response[36] << 8) | controller->command_response[35];
                    gamepad->cal.gyro_rz_offset = (controller->command_response[38] << 8) | controller->command_response[37];
                    break;
                default:
                    break;
            }
            printf("Right gyro X offset: %d\n", gamepad->cal.gyro_rx_offset);
            printf("Right gyro Y offset: %d\n", gamepad->cal.gyro_ry_offset);
            printf("Right gyro Z offset: %d\n", gamepad->cal.gyro_rz_offset);
            break;
        case CNT_WIIMOTE:
            gamepad->cal.gyro_rx_offset = ((controller->command_response[9] << 8) | controller->command_response[10]) - 0x8000;
            gamepad->cal.gyro_ry_offset = ((controller->command_response[11] << 8) | controller->command_response[12]) - 0x8000;
            gamepad->cal.gyro_rz_offset = ((controller->command_response[7] << 8) | controller->command_response[8]) - 0x8000;
            printf("Right gyro X offset: %d\n", gamepad->cal.gyro_rx_offset);
            printf("Right gyro Y offset: %d\n", gamepad->cal.gyro_ry_offset);
            printf("Right gyro Z offset: %d\n", gamepad->cal.gyro_rz_offset);
            break;
        default:
            break;
    }
}

void gamepad_set_cal_gyro_l(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    switch (controller->type) {
        case CNT_JOYCON_L:
            switch (controller->cal_gyro_l_type) {
                case 0:
                    if ((controller->command_response[21] == 0xB2) && (controller->command_response[22] == 0xA1)) { // Magic identifier for user calibration
                        gamepad->cal.gyro_lx_offset = (controller->command_response[36] << 8) | controller->command_response[35];
                        gamepad->cal.gyro_ly_offset = (controller->command_response[38] << 8) | controller->command_response[37];
                        gamepad->cal.gyro_lz_offset = (controller->command_response[40] << 8) | controller->command_response[39];
                    } else {
                        controller->cal_gyro_l_type = 1;
                        hid_queue_command(controller, &cmd_joycon_read_cal_imu_factory, NULL, &gamepad_set_cal_imu, 1);
                    }
                    break;
                case 1:
                    gamepad->cal.gyro_lx_offset = (controller->command_response[34] << 8) | controller->command_response[33];
                    gamepad->cal.gyro_ly_offset = (controller->command_response[36] << 8) | controller->command_response[35];
                    gamepad->cal.gyro_lz_offset = (controller->command_response[38] << 8) | controller->command_response[37];
                    break;
                default:
                    break;
            }
            printf("Left gyro X offset: %d\n", gamepad->cal.gyro_lx_offset);
            printf("Left gyro Y offset: %d\n", gamepad->cal.gyro_ly_offset);
            printf("Left gyro Z offset: %d\n", gamepad->cal.gyro_lz_offset);
            break;
        default:
            break;
    }
}

void gamepad_set_cal_joy_r(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    uint8_t i, checksum = 0;
    switch (controller->type) {
        case CNT_JOYCON_R:
        case CNT_PROCON:
            switch (controller->cal_joy_r_type) {
                case 0:
                    if ((controller->command_response[21] == 0xB2) && (controller->command_response[22] == 0xA1)) { // Magic identifier for user calibration
                        gamepad->cal.joy_rx_center = ((controller->command_response[24] << 8) & 0xF00) | controller->command_response[23];
                        gamepad->cal.joy_ry_center = (controller->command_response[25] << 4) | (controller->command_response[23] >> 4);
                        gamepad->cal.joy_rx_min = gamepad->cal.joy_rx_center - (((controller->command_response[27] << 8) & 0xF00) | controller->command_response[26]);
                        gamepad->cal.joy_rx_max = gamepad->cal.joy_rx_center + (((controller->command_response[30] << 8) & 0xF00) | controller->command_response[29]);
                        gamepad->cal.joy_ry_min = gamepad->cal.joy_ry_center - ((controller->command_response[28] << 4) | (controller->command_response[27] >> 4));
                        gamepad->cal.joy_ry_max = gamepad->cal.joy_ry_center + ((controller->command_response[31] << 4) | (controller->command_response[30] >> 4));
                    } else {
                        controller->cal_joy_r_type = 1;
                        hid_queue_command(controller, &cmd_joycon_read_cal_joy_r_factory, NULL, &gamepad_set_cal_joy_r, 1);
                    }
                    break;
                case 1:
                    gamepad->cal.joy_rx_center = ((controller->command_response[22] << 8) & 0xF00) | controller->command_response[21];
                    gamepad->cal.joy_ry_center = (controller->command_response[23] << 4) | (controller->command_response[22] >> 4);
                    gamepad->cal.joy_rx_min = gamepad->cal.joy_rx_center - (((controller->command_response[25] << 8) & 0xF00) | controller->command_response[24]);
                    gamepad->cal.joy_rx_max = gamepad->cal.joy_rx_center + (((controller->command_response[28] << 8) & 0xF00) | controller->command_response[27]);
                    gamepad->cal.joy_ry_min = gamepad->cal.joy_ry_center - ((controller->command_response[26] << 4) | (controller->command_response[25] >> 4));
                    gamepad->cal.joy_ry_max = gamepad->cal.joy_ry_center + ((controller->command_response[29] << 4) | (controller->command_response[28] >> 4));
                    break;
                default:
                    break;
            }
            printf("Right stick X center: %d\n", gamepad->cal.joy_rx_center);
            printf("Right stick Y center: %d\n", gamepad->cal.joy_ry_center);
            printf("Right stick X min: %d\n", gamepad->cal.joy_rx_min);
            printf("Right stick X max: %d\n", gamepad->cal.joy_rx_max);
            printf("Right stick Y min: %d\n", gamepad->cal.joy_ry_min);
            printf("Right stick Y max: %d\n", gamepad->cal.joy_ry_max);
            break;
        case CNT_WIIMOTE:
            for(i = 0; i < 14; i++) checksum += controller->command_response[7 + i];
            checksum += 0x55;
            if (checksum == controller->command_response[21]) {
                gamepad->cal.joy_rx_max = controller->command_response[13];
                gamepad->cal.joy_rx_min = controller->command_response[14];
                gamepad->cal.joy_rx_center = controller->command_response[15];
                gamepad->cal.joy_ry_max = controller->command_response[16];
                gamepad->cal.joy_ry_min = controller->command_response[17];
                gamepad->cal.joy_ry_center = controller->command_response[18];
            } else {
                gamepad->cal.joy_rx_max = 0xE0;
                gamepad->cal.joy_rx_min = 0x20;
                gamepad->cal.joy_rx_center = 0x80;
                gamepad->cal.joy_ry_max = 0xE0;
                gamepad->cal.joy_ry_min = 0x20;
                gamepad->cal.joy_ry_center = 0x80;
            }
            printf("Right stick X center: %d\n", gamepad->cal.joy_rx_center);
            printf("Right stick Y center: %d\n", gamepad->cal.joy_ry_center);
            printf("Right stick X min: %d\n", gamepad->cal.joy_rx_min);
            printf("Right stick X max: %d\n", gamepad->cal.joy_rx_max);
            printf("Right stick Y min: %d\n", gamepad->cal.joy_ry_min);
            printf("Right stick Y max: %d\n", gamepad->cal.joy_ry_max);
            break;
        case CNT_WIIU_PRO:
            break;
        default:
            break;
    }
}
    
void gamepad_set_cal_joy_l(hid_controller_t * controller) {
    gamepad_t * gamepad = &gamepads[controller->gamepad_num - 1];
    uint8_t i, checksum = 0;
    switch (controller->type) {
        case CNT_JOYCON_L:
        case CNT_PROCON:
            switch (controller->cal_joy_l_type) {
                case 0:
                    if ((controller->command_response[21] == 0xB2) && (controller->command_response[22] == 0xA1)) { // Magic identifier for user calibration
                        gamepad->cal.joy_lx_center = ((controller->command_response[27] << 8) & 0xF00) | controller->command_response[26];
                        gamepad->cal.joy_ly_center = (controller->command_response[28] << 4) | (controller->command_response[27] >> 4);
                        gamepad->cal.joy_lx_min = gamepad->cal.joy_lx_center - (((controller->command_response[30] << 8) & 0xF00) | controller->command_response[29]);
                        gamepad->cal.joy_lx_max = gamepad->cal.joy_lx_center + (((controller->command_response[24] << 8) & 0xF00) | controller->command_response[23]);
                        gamepad->cal.joy_ly_min = gamepad->cal.joy_ly_center - ((controller->command_response[31] << 4) | (controller->command_response[30] >> 4));
                        gamepad->cal.joy_ly_max = gamepad->cal.joy_ly_center + ((controller->command_response[25] << 4) | (controller->command_response[24] >> 4));
                    } else {
                        controller->cal_joy_l_type = 1;
                        hid_queue_command(controller, &cmd_joycon_read_cal_joy_l_factory, NULL, &gamepad_set_cal_joy_l, 1);
                    }
                    break;
                case 1:
                    gamepad->cal.joy_lx_center = ((controller->command_response[25] << 8) & 0xF00) | controller->command_response[24];
                    gamepad->cal.joy_ly_center = (controller->command_response[26] << 4) | (controller->command_response[25] >> 4);
                    gamepad->cal.joy_lx_min = gamepad->cal.joy_lx_center - (((controller->command_response[28] << 8) & 0xF00) | controller->command_response[27]);
                    gamepad->cal.joy_lx_max = gamepad->cal.joy_lx_center + (((controller->command_response[22] << 8) & 0xF00) | controller->command_response[21]);
                    gamepad->cal.joy_ly_min = gamepad->cal.joy_ly_center - ((controller->command_response[29] << 4) | (controller->command_response[28] >> 4));
                    gamepad->cal.joy_ly_max = gamepad->cal.joy_ly_center + ((controller->command_response[23] << 4) | (controller->command_response[22] >> 4));
                    break;
                default:
                    break;
            }
            printf("Left stick X center: %d\n", gamepad->cal.joy_lx_center);
            printf("Left stick Y center: %d\n", gamepad->cal.joy_ly_center);
            printf("Left stick X min: %d\n", gamepad->cal.joy_lx_min);
            printf("Left stick X max: %d\n", gamepad->cal.joy_lx_max);
            printf("Left stick Y min: %d\n", gamepad->cal.joy_ly_min);
            printf("Left stick Y max: %d\n", gamepad->cal.joy_ly_max);
            break;
        case CNT_WIIMOTE:
            for(i = 0; i < 14; i++) checksum += controller->command_response[7 + i];
            checksum += 0x55;
            if (checksum == controller->command_response[21]) {
                switch (controller->extension_type) {
                    case EXT_NUNCHUK:
                        gamepad->cal.joy_lx_max = controller->command_response[15];
                        gamepad->cal.joy_lx_min = controller->command_response[16];
                        gamepad->cal.joy_lx_center = controller->command_response[17];
                        gamepad->cal.joy_ly_max = controller->command_response[18];
                        gamepad->cal.joy_ly_min = controller->command_response[19];
                        gamepad->cal.joy_ly_center = controller->command_response[20];
                        break;
                    case EXT_CLASSIC:
                        gamepad->cal.joy_lx_max = controller->command_response[7];
                        gamepad->cal.joy_lx_min = controller->command_response[8];
                        gamepad->cal.joy_lx_center = controller->command_response[9];
                        gamepad->cal.joy_ly_max = controller->command_response[10];
                        gamepad->cal.joy_ly_min = controller->command_response[11];
                        gamepad->cal.joy_ly_center = controller->command_response[12];
                        
                        break;
                    default:
                        break;
                }
            } else {
                gamepad->cal.joy_lx_max = 0xE0;
                gamepad->cal.joy_lx_min = 0x20;
                gamepad->cal.joy_lx_center = 0x80;
                gamepad->cal.joy_ly_max = 0xE0;
                gamepad->cal.joy_ly_min = 0x20;
                gamepad->cal.joy_ly_center = 0x80;
            }
            printf("Left stick X center: %d\n", gamepad->cal.joy_lx_center);
            printf("Left stick Y center: %d\n", gamepad->cal.joy_ly_center);
            printf("Left stick X min: %d\n", gamepad->cal.joy_lx_min);
            printf("Left stick X max: %d\n", gamepad->cal.joy_lx_max);
            printf("Left stick Y min: %d\n", gamepad->cal.joy_ly_min);
            printf("Left stick Y max: %d\n", gamepad->cal.joy_ly_max);
            break;
        case CNT_WIIU_PRO:
            break;
        default:
            break;
    }
}