#ifndef _HID_CONTROLLER_H_
#define	_HID_CONTROLLER_H_

#define HID_CONTROL_PSM 0x0011
#define HID_INTERRUPT_PSM 0x0013

// Forward declarations
typedef struct hid_controller_t hid_controller_t;
typedef struct hid_command_t hid_command_t;

typedef void (*command_response_cb)(hid_controller_t*);

enum CMD_NAME { // Joy-Con command names
				REPORT_MODE_FULL, REPORT_MODE_STANDARD, ENABLE_IMU, GET_DEVICE_INFO, 
				READ_CAL_IMU_FACTORY, READ_CAL_JOY_L_FACTORY, READ_CAL_JOY_R_FACTORY, 
				READ_CAL_IMU_USER, READ_CAL_JOY_L_USER, READ_CAL_JOY_R_USER, 
				WRITE_CAL_IMU_USER, WRITE_CAL_JOY_L_USER, WRITE_CAL_JOY_R_USER, 
				RESET_PAIR_INFO, PAIR_BEGIN, PAIR_GET_KEY, PAIR_SAVE_KEY, ENABLE_RUMBLE,

				// Wiimote command names
                GET_STATUS, REPORT_MODE_BUTTONS, REPORT_MODE_ACC, REPORT_MODE_EXT8, 
				REPORT_MODE_ACC_IR12, REPORT_MODE_EXT19, REPORT_MODE_ACC_EXT16, REPORT_MODE_IR10_EXT9, 
				REPORT_MODE_ACC_IR10_EXT6, REPORT_MODE_EXT21, READ_EXTENSION_ID, READ_WMP_ID, 
				SETUP_EXTENSION_1, SETUP_EXTENSION_2, SETUP_WMP, ACTIVATE_WMP, SETUP_WMP_PASSTHROUGH_NUNCHUK, SETUP_WMP_PASSTHROUGH_CLASSIC,
				READ_CAL_EXTENSION, READ_CAL_WMP, READ_CAL_ACCEL, WRITE_CAL_WMP, WRITE_CAL_EXTENSION, WRITE_CAL_ACCEL,

				// Shared command names
				SET_PLAYER_LEDS, RUMBLE };
struct hid_command_t {
    enum CMD_NAME name;
    uint8_t response_required;
    command_response_cb response_cb;
    uint8_t len;
	uint8_t arg_len;
	uint8_t arg_offset;
    uint8_t cmd[30];
};

enum EXTENSION_TYPE { EXT_NONE = 0, EXT_NUNCHUK = 1, EXT_CLASSIC = 2, EXT_UNSUPPORTED = 3 };
enum WMP_TYPE { WMP_UNDETERMINED, WMP_NOT_SUPPORTED, WMP_SUPPORTED };
enum HID_DEVICE { CNT_NONE = 0, CNT_JOYCON_R = 1, CNT_JOYCON_L = 2, CNT_PROCON = 3, CNT_WIIMOTE = 4, CNT_WIIU_PRO = 5 };
struct hid_controller_t {
	enum HID_DEVICE type;
	uint8_t address[6];
	uint16_t handle;	// HCI connection handle
	uint16_t setup_complete;
	uint8_t registered;
	uint16_t l2cap_control_cid;
	uint16_t l2cap_interrupt_cid;
	uint8_t connected;
	uint32_t time_connected;
	uint8_t pairing;

	uint8_t gamepad_num;
	uint8_t player_num;
	uint8_t led_state;	// Different controllers use different bits for LEDs

	// For Wiimotes only
	enum EXTENSION_TYPE extension_type;
	enum WMP_TYPE wmp_type;
	uint8_t wmp_active;

	// Flags used once upon connection
	uint8_t cal_accel_r_type;	// 0 = user, 1 = factory
	uint8_t cal_accel_l_type;
	uint8_t cal_gyro_r_type;
	uint8_t cal_gyro_l_type;
	uint8_t cal_joy_r_type;
	uint8_t cal_joy_l_type;

	uint8_t rumble;
	uint16_t rumble_pattern_on_period;
	uint16_t rumble_pattern_off_period;
	uint8_t rumble_pattern_repetitions;
	uint64_t rumble_pattern_timer;
	
	uint8_t status_response[64];	// Holds status responses (button/axis data)
	uint8_t status_response_len;
	uint8_t command_response[64];	// Holds general command responses
	uint8_t command_response_len;
	uint8_t status_response_received;	// Set when status is received, cleared when status is processed
	uint8_t command_response_received;	// Set when command response is received, cleared when data is processed
	uint8_t command_response_verified;	// Set when response is confirmed to match the previous command

	hid_command_t command_buffer[32];	// Holds queued commands
	uint8_t command_buffer_pos;
	uint8_t command_queue_num;
	uint8_t command_send_event_queued;
	uint8_t command_packet_count;
};

// HID handling
void hid_queue_command(hid_controller_t * controller, const hid_command_t * command, uint8_t * arg, command_response_cb response_cb, uint8_t response_required);
uint8_t hid_send_next_command(hid_controller_t * controller);
void hid_get_response(hid_controller_t * controller, uint8_t * response, uint8_t response_size);
uint8_t hid_response_matches_command(hid_controller_t * controller, hid_command_t * command);
void hid_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// Controller handling
void init_controllers();
void controller_reset(hid_controller_t * controller);
hid_controller_t * get_controller(uint8_t controller_num);
hid_controller_t * get_controller_from_cid(uint16_t cid);
hid_controller_t * get_controller_from_addr(uint8_t * addr);
void get_controllers_from_gamepad(uint8_t gamepad_num, hid_controller_t ** controller_main, hid_controller_t ** controller_secondary, uint8_t connection_required);
hid_controller_t * register_controller(enum HID_DEVICE controller_type, uint8_t * addr);
void controller_detect_extension(hid_controller_t * controller);
void controller_setup_extension(hid_controller_t * controller);
void controller_setup_motion_plus(hid_controller_t * controller);
void controller_set_leds(hid_controller_t * controller, uint8_t player_num);
void controller_rumble(hid_controller_t * controller, uint8_t enable);
void controller_rumble_pattern(hid_controller_t * controller, uint16_t on_period, uint16_t off_period, uint8_t repetitions);
void controller_rumble_handle(hid_controller_t * controller);
void controller_setup(hid_controller_t * controller);
void controller_handle(uint8_t controller_num);
void controller_disconnect(hid_controller_t * controller);

#endif