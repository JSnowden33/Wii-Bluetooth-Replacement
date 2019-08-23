#include <inttypes.h>
#include <stdio.h>
#include "btstack.h"
#include "gamepad.h"
#include "hid_command.h"
#include "hid_controller.h"
#include "pair.h"
#include "uart_controller.h"
#include "timer.h"
#include "driver/timer.h"

hid_controller_t controllers[8];

const uint8_t rumble_off_data[8] = { 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };
const uint8_t rumble_on_data[8] = { 0x00, 0x00, 0x4B, 0x60, 0x00, 0x00, 0x4B, 0x60 };	// Medium-force rumble using low-band only (about 200 Hz)

const uint8_t ext_id_nunchuk[6] = { 0x00, 0x00, 0xA4, 0x20, 0x00, 0x00 };
const uint8_t ext_id_classic[6] = { 0x00, 0x00, 0xA4, 0x20, 0x01, 0x01 };
const uint8_t ext_id_classic_pro[6] = { 0x01, 0x00, 0xA4, 0x20, 0x01, 0x01 };
const uint8_t ext_id_wiiu_pro[6] = { 0x00, 0x00, 0xA4, 0x20, 0x01, 0x20 };

const uint8_t ext_id_wmp_inactive[6] = { 0x00, 0x00, 0xA6, 0x20, 0x00, 0x05 };
const uint8_t ext_id_wmp_active[6] = { 0x00, 0x00, 0xA4, 0x20, 0x04, 0x05 };
const uint8_t ext_id_wmp_nunchuk[6] = { 0x00, 0x00, 0xA4, 0x20, 0x05, 0x05 };
const uint8_t ext_id_wmp_classic[6] = { 0x00, 0x00, 0xA4, 0x20, 0x07, 0x05 };

void hid_queue_command(hid_controller_t * controller, const hid_command_t * command, uint8_t * arg, command_response_cb response_cb, uint8_t response_required) {
	hid_command_t * cur_command = &controller->command_buffer[(controller->command_buffer_pos + controller->command_queue_num) % 32];
	memcpy(cur_command, command, sizeof(hid_command_t));
	cur_command->response_required = response_required;
	cur_command->response_cb = response_cb;	// Register callback for response

	// Add command arguments if provided
	if (arg) {
		memcpy(cur_command->cmd + cur_command->arg_offset, arg, cur_command->arg_len);
		if ((cur_command->arg_offset + cur_command->arg_len) > cur_command->len) cur_command->len += (cur_command->arg_offset + cur_command->arg_len - cur_command->len);
	}

	controller->command_queue_num++;
	//printf("%d reports queued\n", controller->command_queue_num);
}

uint8_t hid_send_next_command(hid_controller_t * controller) {
	if (controller->command_queue_num) {
		hid_command_t * command = &controller->command_buffer[controller->command_buffer_pos];
		switch (controller->type) {
			case CNT_JOYCON_R:
			case CNT_JOYCON_L:
			case CNT_PROCON:
				if (command->cmd[1] == 0x01 || command->cmd[1] == 0x10) {	// Check if subcommand or rumble command
					command->cmd[2] = controller->command_packet_count;
					controller->command_packet_count = (controller->command_packet_count + 1) % 0x10;
					if (controller->rumble) memcpy(command->cmd + 3, rumble_on_data, 8);
					else memcpy(command->cmd + 3, rumble_off_data, 8);
				}
				break;
			case CNT_WIIMOTE:
			case CNT_WIIU_PRO:
				if (controller->rumble) command->cmd[2] |= 0x01;	// Turn rumble on
				else command->cmd[2] &= 0xFE;	// Turn rumble off
				break;
			default:
				break;
		}

		printf("%llu - ", app_timer);
		printf_hexdump(command->cmd, command->len);
		
		controller->command_send_event_queued = 1;
		controller->command_queue_num--;
		l2cap_request_can_send_now_event(controller->l2cap_interrupt_cid);
		return 1;
	}
	return 0;
}

void hid_get_response(hid_controller_t * controller, uint8_t * response, uint8_t response_size) {
	if ((response[1] & 0x30) == 0x30) {
		memset(controller->status_response, 0, 64);
		memcpy(controller->status_response, response, response_size);
		controller->status_response_len = response_size;
		controller->status_response_received = 1;
		gamepad_get_angles_from_controller(controller);	// Angle calculations performed here to avoid latency
	} else {
		memset(controller->command_response, 0, 64);
		memcpy(controller->command_response, response, response_size);
		controller->command_response_len = response_size;
		controller->command_response_received = 1;
		//printf("Command response received\n");
		printf("%llu - ", app_timer);
		printf_hexdump(controller->command_response, controller->command_response_len);

		// Check if new response belongs to previous command
		if (hid_response_matches_command(controller, &controller->command_buffer[(controller->command_buffer_pos - 1) % 32]))
			controller->command_response_verified = 1;
	}

	switch (controller->type) {
		case CNT_JOYCON_R:
		case CNT_JOYCON_L:
		case CNT_PROCON:
			break;
		case CNT_WIIMOTE:
			// Check WMP extension status
			if ((response[1] == 0x37) && (response[22] != 0xFF) && (controller->wmp_type == WMP_SUPPORTED) && controller->wmp_active) {	// If byte 22 is 0xFF then WMP is not sending data yet
				if ((response[21] & 0x01) && (controller->extension_type == EXT_NONE))	// Extension plugged in
					controller_detect_extension(controller);	
				if (!(response[21] & 0x01) && (controller->extension_type != EXT_NONE))	// Extension unplugged
					controller_detect_extension(controller);
			}
			// Check general extension status (report mode must always be updated here)
			else if (response[1] == 0x20) {
				if (controller->wmp_type == WMP_NOT_SUPPORTED) controller_detect_extension(controller);
				else if (controller->wmp_type == WMP_SUPPORTED) hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir10_ext6, NULL, NULL, 1);
				else hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir12, NULL, NULL, 1);
			}
			break;
		default:
			break;
	}
}

uint8_t hid_response_matches_command(hid_controller_t * controller, hid_command_t * command) {
	switch (controller->type) {
		case CNT_JOYCON_R:
		case CNT_JOYCON_L:
		case CNT_PROCON:
			switch (command->name) {
				// SPI flash read commands
				case READ_CAL_IMU_FACTORY:
				case READ_CAL_JOY_L_FACTORY:
				case READ_CAL_JOY_R_FACTORY:
				case READ_CAL_IMU_USER:
				case READ_CAL_JOY_L_USER:
				case READ_CAL_JOY_R_USER:
					if (!memcmp(controller->command_response + 15, command->cmd + 11, 6)) return 1;
					break;
				// Standard subcommands
				default:
					if (!memcmp(controller->command_response + 15, command->cmd + 11, 1)) return 1;
					break;
			}
			break;
		case CNT_WIIMOTE:
		case CNT_WIIU_PRO:
			switch (command->name) {
				// Status command
				case GET_STATUS:
					if (controller->command_response[1] == 0x20) return 1;
					break;
				// I2C flash read commands
				case READ_EXTENSION_ID:
				case READ_WMP_ID:
				case READ_CAL_EXTENSION:
				case READ_CAL_WMP:
				case READ_CAL_ACCEL:
					if ((controller->command_response[1] == 0x21) && !memcmp(controller->command_response + 5, command->cmd + 4, 2)) return 1;
					break;
				// Standard command response
				default:
					if ((controller->command_response[1] == 0x22) && (controller->command_response[4] == command->cmd[1])) return 1;
					break;
			}
			break;
		default:
			break;
	}
	return 0;
}
	
void hid_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    uint8_t event;
    uint16_t l2cap_cid;
	hid_controller_t * controller = NULL;

    switch (packet_type) {
		case HCI_EVENT_PACKET:
			event = hci_event_packet_get_type(packet);
			if (event == L2CAP_EVENT_CAN_SEND_NOW) {
				l2cap_cid = l2cap_event_can_send_now_get_local_cid(packet);
				controller = get_controller_from_cid(l2cap_cid);
				if (controller) {
					if (controller->command_send_event_queued) {
						l2cap_send(l2cap_cid, controller->command_buffer[controller->command_buffer_pos].cmd, controller->command_buffer[controller->command_buffer_pos].len);
						controller->command_buffer_pos = (controller->command_buffer_pos + 1) % 32;
						controller->command_send_event_queued = 0;
						//printf("Output report sent\n");
					}
				}
			}
			break;
		case L2CAP_DATA_PACKET:
			controller = get_controller_from_cid(channel);
			if (controller) hid_get_response(controller, packet, size);
			break;
        default:
            break;
    }
}

void init_controllers() {
	controller_reset(&controllers[0]);
	controller_reset(&controllers[1]);
	controller_reset(&controllers[2]);
	controller_reset(&controllers[3]);
	controller_reset(&controllers[4]);
	controller_reset(&controllers[5]);
	controller_reset(&controllers[6]);
	controller_reset(&controllers[7]);
}

void controller_reset(hid_controller_t * controller) {
	memset(controller, 0, sizeof(hid_controller_t));
	controller->type = CNT_NONE;
	controller->extension_type = EXT_NONE;
	controller->wmp_type = WMP_UNDETERMINED;
}

hid_controller_t * get_controller(uint8_t controller_num) {
	if (controller_num > 0 && controller_num <= 8) return &controllers[controller_num - 1];
	return NULL;
}

hid_controller_t * get_controller_from_cid(uint16_t cid) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		if (controllers[i].registered && (controllers[i].l2cap_control_cid == cid || controllers[i].l2cap_interrupt_cid == cid)) return &controllers[i];
	}
	return NULL;
}

hid_controller_t * get_controller_from_addr(uint8_t * addr) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		if (controllers[i].registered && !memcmp(controllers[i].address, addr, 6)) return &controllers[i];
	}
	return NULL;
}

void get_controllers_from_gamepad(uint8_t gamepad_num, hid_controller_t ** controller_main, hid_controller_t ** controller_secondary, uint8_t connection_required) {
	uint8_t i;
	*controller_main = NULL;
	*controller_secondary = NULL;
	for (i = 0; i < 8; i++) {
		if (controllers[i].gamepad_num == gamepad_num) {
			switch (controllers[i].type) {
				case CNT_JOYCON_R:
				case CNT_PROCON:
				case CNT_WIIMOTE:
				case CNT_WIIU_PRO:
					if (connection_required) {
						if (controllers[i].connected && controllers[i].registered) *controller_main = &controllers[i];
					} else if (controllers[i].registered) *controller_main = &controllers[i];
					break;
				case CNT_JOYCON_L:
					if (connection_required) {
						if (controllers[i].connected && controllers[i].registered) *controller_secondary = &controllers[i];
					} else if (controllers[i].registered) *controller_secondary = &controllers[i];
					break;
				default:
					break;
			}
		}
	}
}

// Assign new controller to lowest available gamepad number
hid_controller_t * register_controller(enum HID_DEVICE controller_type, uint8_t * addr) {
	if (get_controller_from_addr(addr)) return NULL;	// Controller with this address already registered
	
	if (controller_type == CNT_NONE) controller_type = get_controller_type(addr);
	else store_controller_type(controller_type, addr);
	if (controller_type == CNT_NONE) return NULL;	// No controller type found for this address

	uint8_t i, gamepad_num, is_second_joycon = 0;
	hid_controller_t * controller_main = NULL;
	hid_controller_t * controller_secondary = NULL;
	for (gamepad_num = 1; gamepad_num < 5; gamepad_num++) {
		if ((gamepad_num == UART_GAMEPAD_NUM) && (joycon_right.data_ready || joycon_left.data_ready)) gamepad_num++;	// Skip this gamepad if wired Joy-Con are connected to it
		get_controllers_from_gamepad(gamepad_num, &controller_main, &controller_secondary, 0);
		if (!controller_main && !controller_secondary) break;	// No controllers for this gamepad slot
		if (!controller_main) {
			if (controller_secondary->type == CNT_JOYCON_L && 
				controller_type == CNT_JOYCON_R && 
				gamepads[gamepad_num - 1].type != GAMEPAD_JOYCON_L_SIDE) {
				is_second_joycon = 1;	// This gamepad slot needs a right Joy-Con
				break;	
			}
		}
		if (!controller_secondary) {
			if (controller_main->type == CNT_JOYCON_R && 
				controller_type == CNT_JOYCON_L && 
				gamepads[gamepad_num - 1].type != GAMEPAD_JOYCON_R_SIDE) {
				is_second_joycon = 1;	// This gamepad slot needs a left Joy-Con
				break;	
			}
		}
	}

	printf("Registering new controller with gamepad %d\n", gamepad_num);
	for (i = 0; i < 8; i++) {
		if (!controllers[i].registered && gamepad_num <= 4) {
			controller_reset(&controllers[i]);
			memcpy(controllers[i].address, addr, 6);
			controllers[i].type = controller_type;
			controllers[i].gamepad_num = gamepad_num;
			controllers[i].registered = 1;
			if (!is_second_joycon) gamepad_reset(&gamepads[gamepad_num - 1]);	// Do not erase gamepad settings if another Joy-Con is actively using it
			gamepad_set_type(gamepad_num);
			gamepad_set_extension(gamepad_num);
			return &controllers[i];
		}
	}
	return NULL;
}

// Called when WMP is activated
static void controller_activate_motion_plus_cb(hid_controller_t * controller) {
	controller->wmp_active = 1;
	printf("WMP activated\n");
}

// Called when Wiimote or WMP reports extension change/general status
void controller_detect_extension(hid_controller_t * controller) {
	if (controller->wmp_type == WMP_NOT_SUPPORTED) {
		if (controller->extension_type == EXT_NONE) {
			if (controller->command_response[4] & 0x02) {
				hid_queue_command(controller, &cmd_wiimote_setup_extension_1, NULL, NULL, 1);
				hid_queue_command(controller, &cmd_wiimote_setup_extension_2, NULL, NULL, 1);
				hid_queue_command(controller, &cmd_wiimote_read_extension_id, NULL, &controller_setup_extension, 1);
				hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir10_ext6, NULL, NULL, 1);
				printf("Extension plugged in\n");
			} 
			else hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir12, NULL, NULL, 1);
		} else {
			if (!(controller->command_response[4] & 0x02)) {
				controller->extension_type = EXT_NONE;
				hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir12, NULL, NULL, 1);
				printf("Extension unplugged\n");
			} 
			else hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir10_ext6, NULL, NULL, 1);
		}
	} else if (controller->wmp_type == WMP_SUPPORTED) {
		if (controller->extension_type == EXT_NONE) {
			controller->wmp_active = 0;
			hid_queue_command(controller, &cmd_wiimote_setup_extension_1, NULL, NULL, 1);	// Disable WMP to read extension information
			hid_queue_command(controller, &cmd_wiimote_setup_extension_2, NULL, NULL, 1);
			hid_queue_command(controller, &cmd_wiimote_read_extension_id, NULL, &controller_setup_extension, 1);	// Reactivate WMP in callback 
			printf("Extension plugged in (WMP)\n");
		} else {
			controller->extension_type = EXT_NONE;
			controller->wmp_active = 0;
			hid_queue_command(controller, &cmd_wiimote_setup_extension_1, NULL, NULL, 1);	// Disable WMP to read extension information
			hid_queue_command(controller, &cmd_wiimote_activate_wmp, NULL, &controller_activate_motion_plus_cb, 1);	// Rectivate WMP with no passthrough
			printf("Extension unplugged (WMP)\n");
		}
	}
}

// Called when Wiimote responds with extension ID
void controller_setup_extension(hid_controller_t * controller) {
	if (!memcmp(controller->command_response + 7, ext_id_nunchuk, 6)) {
		controller->extension_type = EXT_NUNCHUK;
		hid_queue_command(controller, &cmd_wiimote_read_cal_extension, NULL, &gamepad_set_cal_nunchuk, 1);
		if (controller->wmp_type == WMP_SUPPORTED) hid_queue_command(controller, &cmd_wiimote_activate_wmp_passthrough_nunchuk, NULL, &controller_activate_motion_plus_cb, 1);
	}
	else if (!memcmp(controller->command_response + 7, ext_id_classic, 6) || !memcmp(controller->command_response + 7, ext_id_classic_pro, 6)) {
		controller->extension_type = EXT_CLASSIC;
		hid_queue_command(controller, &cmd_wiimote_read_cal_extension, NULL, &gamepad_set_cal_classic, 1);
		if (controller->wmp_type == WMP_SUPPORTED) hid_queue_command(controller, &cmd_wiimote_activate_wmp_passthrough_classic, NULL, &controller_activate_motion_plus_cb, 1);
	} else {
		controller->extension_type = EXT_UNSUPPORTED;
		if (controller->wmp_type == WMP_SUPPORTED) hid_queue_command(controller, &cmd_wiimote_activate_wmp, NULL, &controller_activate_motion_plus_cb, 1);
	}
}

// Called when Wiimote responds with WMP ID
void controller_setup_motion_plus(hid_controller_t * controller) {
	if (!memcmp(controller->command_response + 11, ext_id_wmp_inactive + 4, 2)) {
		controller->wmp_type = WMP_SUPPORTED;
		hid_queue_command(controller, &cmd_wiimote_setup_wmp, NULL, NULL, 1);
		hid_queue_command(controller, &cmd_wiimote_read_cal_wmp, NULL, &gamepad_set_cal_gyro_r, 1);
		hid_queue_command(controller, &cmd_wiimote_activate_wmp, NULL, &controller_activate_motion_plus_cb, 1);
		printf("Wii Motion Plus found\n");
	} else {
		controller->wmp_type = WMP_NOT_SUPPORTED;
		hid_queue_command(controller, &cmd_wiimote_get_status, NULL, NULL, 1);	// Check for extension missed during setup
		printf("Wii Motion Plus not found\n");
	}
}

void controller_set_leds(hid_controller_t * controller, uint8_t player_num) {
	if (controller->player_num != player_num) {
		switch (controller->type) {
			case CNT_JOYCON_R:
			case CNT_JOYCON_L:
			case CNT_PROCON:
				switch (player_num) {
					case 1: controller->led_state = 0x01;
						break;
					case 2: controller->led_state = 0x03;
						break;
					case 3: controller->led_state = 0x07;
						break;
					case 4: controller->led_state = 0x0F;
						break;
					default: controller->led_state = 0x00;
						break;
				}
				hid_queue_command(controller, &cmd_joycon_set_player_leds, &controller->led_state, NULL, 1);
				break;
			case CNT_WIIMOTE:
			case CNT_WIIU_PRO:
				switch (player_num) {
					case 1: controller->led_state = 0x12;
						break;
					case 2: controller->led_state = 0x22;
						break;
					case 3: controller->led_state = 0x42;
						break;
					case 4: controller->led_state = 0x82;
						break;
					default: controller->led_state = 0x02;
						break;
				}
				hid_queue_command(controller, &cmd_wiimote_set_player_leds, &controller->led_state, NULL, 1);
				break;
			default:
				break;
		}
		controller->player_num = player_num;
	}
}

void controller_rumble(hid_controller_t * controller, uint8_t enable) {
	controller->rumble = enable & 0x01;
	switch(controller->type) {
		case CNT_JOYCON_R:
		case CNT_JOYCON_L:
		case CNT_PROCON:
			if (!controller->command_queue_num) hid_queue_command(controller, &cmd_joycon_rumble, NULL, NULL, 0);
			break;
		case CNT_WIIMOTE:
		case CNT_WIIU_PRO:
			if (!controller->command_queue_num) hid_queue_command(controller, &cmd_wiimote_rumble, NULL, NULL, 0);
			break;
		default:
			break;
	}
}

// Patterns started when other commands are queued may have altered timing
void controller_rumble_pattern(hid_controller_t * controller, uint16_t on_period, uint16_t off_period, uint8_t repetitions) {
	if (repetitions > 0) {
		controller->rumble_pattern_on_period = on_period;
		controller->rumble_pattern_off_period = off_period;
		controller->rumble_pattern_repetitions = repetitions * 2;
		controller->rumble_pattern_timer = app_timer;

		// Begin pattern
		controller_rumble(controller, 1);
		controller->rumble_pattern_repetitions--;
	}
}

// Callback used to rumble once after controller setup is complete
void controller_connect_rumble(hid_controller_t * controller) {
	switch (controller->type) {
		case CNT_JOYCON_R:
		case CNT_JOYCON_L:
		case CNT_PROCON:
			controller_rumble_pattern(controller, 50, 0, 1);
			break;
		case CNT_WIIMOTE:
			controller_rumble_pattern(controller, 100, 0, 1);
			break;
		case CNT_WIIU_PRO:
			controller_rumble_pattern(controller, 200, 0, 1);
			break;
		default:
			break;
	}
}

void controller_rumble_handle(hid_controller_t * controller) {
	switch (controller->type) {
		case CNT_JOYCON_R:
		case CNT_JOYCON_L:
		case CNT_PROCON:
			if (controller->rumble && !controller->command_queue_num && (app_timer % 250 <= 5)) 
				hid_queue_command(controller, &cmd_joycon_rumble, NULL, NULL, 0);	// Keep rumble on
			break;
		default:
			break;
	}

	if (controller->rumble_pattern_repetitions) {
		if ((controller->rumble_pattern_repetitions % 2 == 1) && (app_timer - controller->rumble_pattern_timer >= controller->rumble_pattern_on_period)) {
			controller_rumble(controller, 0);	// Turn rumble off
			controller->rumble_pattern_repetitions--;
			controller->rumble_pattern_timer = app_timer;
		}
		else if ((controller->rumble_pattern_repetitions % 2 == 0) && (app_timer - controller->rumble_pattern_timer >= controller->rumble_pattern_off_period)) {
			controller_rumble(controller, 1);	// Turn rumble on
			controller->rumble_pattern_repetitions--;
			controller->rumble_pattern_timer = app_timer;
		}
	}
}

// Called on loop to handle outgoing/incoming commands
void controller_handle(uint8_t controller_num) {
	hid_controller_t * controller = &controllers[controller_num - 1];
	if (controller->connected) {
		controller_rumble_handle(controller);
		
		// Log time of connection for new connection
		if (controller->time_connected == 0) controller->time_connected = app_timer;
		
		// Check for command responses and send commands
		// TODO: Set timeout for when proper response isn't received
		if (!controller->command_send_event_queued) {
			hid_command_t * prev_command = &controller->command_buffer[(controller->command_buffer_pos - 1) % 32];
			if (prev_command->response_required) {
				if (controller->command_response_verified) {
					// Use callback when verified response is received
					if (prev_command->response_cb) {
						prev_command->response_cb(controller);
						prev_command->response_cb = NULL;
					}
					// Response is no longer valid when next command is sent
					if (hid_send_next_command(controller)) controller->command_response_verified = 0;
				}	
			} else hid_send_next_command(controller);
		}
	}
}

// Called once upon connection
void controller_setup(hid_controller_t * controller) {
	switch (controller->type) {
		case CNT_JOYCON_R:
		case CNT_JOYCON_L:
		case CNT_PROCON:
			if (controller->pairing) {
				hid_queue_command(controller, &cmd_joycon_pair_begin, host_mac_addr_rev, NULL, 1);
				hid_queue_command(controller, &cmd_joycon_pair_get_key, NULL, &store_link_key, 1);
				hid_queue_command(controller, &cmd_joycon_pair_save_key, NULL, &end_pair, 1);
			}
			hid_queue_command(controller, &cmd_joycon_read_cal_imu_user, NULL, &gamepad_set_cal_imu, 1);
			hid_queue_command(controller, &cmd_joycon_read_cal_joy_r_user, NULL, &gamepad_set_cal_joy_r, 1);
			hid_queue_command(controller, &cmd_joycon_read_cal_joy_l_user, NULL, &gamepad_set_cal_joy_l, 1);
			hid_queue_command(controller, &cmd_joycon_enable_imu, NULL, NULL, 1);
			hid_queue_command(controller, &cmd_joycon_enable_rumble, NULL, NULL, 1);
			//controller_set_leds(controller, controller->gamepad_num);
			hid_queue_command(controller, &cmd_joycon_report_mode_full, NULL, &controller_connect_rumble, 1);
			break;
		case CNT_WIIMOTE:
			if (controller->pairing) end_pair(controller);	// Pin code pairing occurs in HCI event handler
			//hid_queue_command(controller, &cmd_wiimote_report_mode_acc_ir12, NULL, NULL, 1);
			hid_queue_command(controller, &cmd_wiimote_read_wmp_id, NULL, &controller_setup_motion_plus, 1);	// Check for WMP
			hid_queue_command(controller, &cmd_wiimote_read_cal_accel, NULL, &gamepad_set_cal_accel_r, 1);
			//controller_set_leds(controller, controller->gamepad_num);
			hid_queue_command(controller, &cmd_wiimote_get_status, NULL, &controller_connect_rumble, 1);
			break;
		case CNT_WIIU_PRO:
			if (controller->pairing) end_pair(controller);	// Pin code pairing occurs in HCI event handler
			hid_queue_command(controller, &cmd_wiimote_report_mode_ext21, NULL, NULL, 1);
			//controller_set_leds(controller, controller->gamepad_num);
			hid_queue_command(controller, &cmd_wiimote_get_status, NULL, &controller_connect_rumble, 1);
		default:
			break;
	}
}

void controller_disconnect(hid_controller_t * controller) {
	if (!controller->pairing) gap_disconnect(controller->handle);
}